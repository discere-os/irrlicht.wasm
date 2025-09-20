/*
 * WebGPU Intelligent Render Batching Implementation for Irrlicht Engine
 * Copyright (c) 2002-2012 Nikolaus Gebhardt
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license (same as original Irrlicht Engine)
 */

#include "CWebGPURenderBatcher.h"
#include "CWebGPUDriver.h"
#include "os.h"
#include "ITexture.h"

#ifdef _IRR_COMPILE_WITH_WEBGPU_

#include <algorithm>
#include <cstring>

namespace irr {
namespace video {

CWebGPURenderBatcher::CWebGPURenderBatcher(CWebGPUDriver* driver)
    : driver(driver), device(nullptr), queue(nullptr),
      maxBatches(1000), maxBatchSize(65536), autoFlushThreshold(100),
      instancingEnabled(true), autoFlushEnabled(true), debugMode(false),
      currentFrame(0) {

    memset(&stats, 0, sizeof(stats));

    if (driver) {
        device = driver->getWebGPUDevice();
        if (device) {
            queue = wgpuDeviceGetQueue(device);
        }
    }
}

CWebGPURenderBatcher::~CWebGPURenderBatcher() {
    // Cleanup GPU resources
    for (auto& buffer : vertexBufferPool) {
        if (buffer) wgpuBufferRelease(buffer);
    }
    for (auto& buffer : indexBufferPool) {
        if (buffer) wgpuBufferRelease(buffer);
    }
    for (auto& buffer : instanceBufferPool) {
        if (buffer) wgpuBufferRelease(buffer);
    }

    // Cleanup active batches
    for (auto& batch : activeBatches) {
        if (batch.vertexBuffer) wgpuBufferRelease(batch.vertexBuffer);
        if (batch.indexBuffer) wgpuBufferRelease(batch.indexBuffer);
        if (batch.instanceBuffer) wgpuBufferRelease(batch.instanceBuffer);
        if (batch.bindGroup) wgpuBindGroupRelease(batch.bindGroup);
    }
}

bool CWebGPURenderBatcher::initialize(u32 maxBatches, u32 maxBatchSize) {
    if (!device || !queue) {
        os::Printer::log("❌ CWebGPURenderBatcher: No WebGPU device available", ELL_ERROR);
        return false;
    }

    this->maxBatches = maxBatches;
    this->maxBatchSize = maxBatchSize;

    // Reserve memory for batches
    activeBatches.reserve(maxBatches);
    pendingDrawCalls.reserve(maxBatchSize);

    // Pre-allocate some buffer pools
    vertexBufferPool.reserve(50);
    indexBufferPool.reserve(50);
    instanceBufferPool.reserve(50);

    os::Printer::log("✅ CWebGPURenderBatcher: Initialized with smart batching for optimal performance", ELL_INFORMATION);
    return true;
}

void CWebGPURenderBatcher::beginFrame() {
    currentFrame++;

    // Clear pending data
    pendingDrawCalls.clear();
    compatibilityHashToBatch.clear();

    // Clean up old batches
    for (auto it = activeBatches.begin(); it != activeBatches.end();) {
        if (currentFrame - it->lastUsedFrame > 3) { // Keep for 3 frames
            if (it->vertexBuffer) wgpuBufferRelease(it->vertexBuffer);
            if (it->indexBuffer) wgpuBufferRelease(it->indexBuffer);
            if (it->instanceBuffer) wgpuBufferRelease(it->instanceBuffer);
            if (it->bindGroup) wgpuBindGroupRelease(it->bindGroup);
            it = activeBatches.erase(it);
        } else {
            ++it;
        }
    }

    // Reset statistics for new frame
    stats.totalDrawCalls = 0;
    stats.batchedDrawCalls = 0;
    stats.totalBatches = 0;
    stats.verticesProcessed = 0;
    stats.indicesProcessed = 0;
}

void CWebGPURenderBatcher::endFrame() {
    // Flush any remaining batches
    flushBatches();

    // Calculate statistics
    if (stats.totalDrawCalls > 0) {
        stats.batchingEfficiency = (f32)stats.batchedDrawCalls / stats.totalDrawCalls * 100.0f;
    }
    if (stats.totalBatches > 0) {
        stats.averageBatchSize = stats.batchedDrawCalls / stats.totalBatches;
        stats.averageVerticesPerBatch = (f32)stats.verticesProcessed / stats.totalBatches;
    }

    if (debugMode) {
        logBatchingStats();
    }
}

void CWebGPURenderBatcher::addDrawCall(const void* vertices, u32 vertexCount, E_VERTEX_TYPE vType,
                                      const void* indices, u32 indexCount, u32 primitiveCount,
                                      scene::E_PRIMITIVE_TYPE pType, E_INDEX_TYPE iType,
                                      const SMaterial& material, const core::matrix4& worldMatrix) {

    if (!device || !vertices) return;

    stats.totalDrawCalls++;
    stats.verticesProcessed += vertexCount;
    stats.indicesProcessed += indexCount;

    // Create draw call descriptor
    DrawCall call = {};
    call.vertices = vertices;
    call.vertexCount = vertexCount;
    call.vertexType = vType;
    call.indices = indices;
    call.indexCount = indexCount;
    call.primitiveCount = primitiveCount;
    call.primitiveType = pType;
    call.indexType = iType;
    call.material = material;
    call.worldMatrix = worldMatrix;

    // Get view and projection matrices from driver
    // call.viewMatrix = driver->getTransform(ETS_VIEW);
    // call.projectionMatrix = driver->getTransform(ETS_PROJECTION);

    // Calculate compatibility hash
    call.compatibilityHash = calculateCompatibilityHash(call);

    // Add to pending calls
    pendingDrawCalls.push_back(call);

    // Auto-flush if threshold reached
    if (autoFlushEnabled && pendingDrawCalls.size() >= autoFlushThreshold) {
        flushBatches();
    }
}

void CWebGPURenderBatcher::flushBatches() {
    if (pendingDrawCalls.empty()) return;

    // Group draw calls into batches by compatibility
    for (const auto& call : pendingDrawCalls) {
        u32 batchId = findOrCreateBatch(call);
        if (batchId != static_cast<u32>(-1)) {
            addToBatch(batchId, call);
            stats.batchedDrawCalls++;
        } else {
            // Execute immediately if can't batch
            executeImmediateDrawCall(call);
        }
    }

    // Execute all active batches
    for (auto& batch : activeBatches) {
        if (batch.needsUpload && batch.instanceCount > 0) {
            updateBatchBuffers(batch);
            createBatchBindGroup(batch);

            if (instancingEnabled && batch.instanceCount > 1) {
                executeInstancedBatch(batch);
            } else {
                executeBatch(batch);
            }

            batch.lastUsedFrame = currentFrame;
            batch.needsUpload = false;
        }
    }

    stats.totalBatches = static_cast<u32>(activeBatches.size());
    pendingDrawCalls.clear();
}

u64 CWebGPURenderBatcher::calculateCompatibilityHash(const DrawCall& call) const {
    u64 hash = 0;

    // Hash material properties
    hash ^= hashMaterial(call.material);
    hash = (hash << 8) | (hash >> 56); // Rotate

    // Hash vertex type
    hash ^= static_cast<u64>(call.vertexType);
    hash = (hash << 4) | (hash >> 60);

    // Hash primitive type
    hash ^= static_cast<u64>(call.primitiveType);
    hash = (hash << 4) | (hash >> 60);

    // Hash index type
    hash ^= static_cast<u64>(call.indexType);

    return hash;
}

u32 CWebGPURenderBatcher::findOrCreateBatch(const DrawCall& call) {
    // Check if we already have a compatible batch
    auto it = compatibilityHashToBatch.find(call.compatibilityHash);
    if (it != compatibilityHashToBatch.end()) {
        u32 batchId = it->second;

        // Verify the batch has space and is truly compatible
        RenderBatch& batch = activeBatches[batchId];
        if (batch.totalVertices + call.vertexCount <= maxBatchSize &&
            batch.instanceCount < 1024 && // Max instances per batch
            materialsCompatible(batch.material, call.material) &&
            batch.vertexType == call.vertexType &&
            batch.primitiveType == call.primitiveType) {
            return batchId;
        }
    }

    // Create new batch
    if (activeBatches.size() >= maxBatches) {
        return static_cast<u32>(-1); // Can't create more batches
    }

    RenderBatch newBatch = {};
    newBatch.material = call.material;
    newBatch.vertexType = call.vertexType;
    newBatch.primitiveType = call.primitiveType;
    newBatch.indexType = call.indexType;
    newBatch.totalVertices = 0;
    newBatch.totalIndices = 0;
    newBatch.instanceCount = 0;
    newBatch.needsUpload = true;
    newBatch.lastUsedFrame = currentFrame;
    newBatch.vertexBuffer = nullptr;
    newBatch.indexBuffer = nullptr;
    newBatch.instanceBuffer = nullptr;
    newBatch.bindGroup = nullptr;

    u32 batchId = static_cast<u32>(activeBatches.size());
    activeBatches.push_back(newBatch);
    compatibilityHashToBatch[call.compatibilityHash] = batchId;

    return batchId;
}

void CWebGPURenderBatcher::addToBatch(u32 batchId, const DrawCall& call) {
    if (batchId >= activeBatches.size()) return;

    RenderBatch& batch = activeBatches[batchId];

    // Calculate sizes
    size_t vertexSize = getVertexSize(call.vertexType);
    size_t indexSize = getIndexSize(call.indexType);

    // Calculate current offsets
    u32 vertexOffset = batch.totalVertices;
    u32 indexOffset = batch.totalIndices;

    // Add vertex data
    size_t vertexDataSize = call.vertexCount * vertexSize;
    size_t currentVertexSize = batch.vertexData.size();
    batch.vertexData.resize(currentVertexSize + vertexDataSize);
    memcpy(batch.vertexData.data() + currentVertexSize, call.vertices, vertexDataSize);

    // Add index data (with vertex offset adjustment)
    if (call.indices && call.indexCount > 0) {
        size_t indexDataSize = call.indexCount * indexSize;
        size_t currentIndexSize = batch.indexData.size();
        batch.indexData.resize(currentIndexSize + indexDataSize);

        // Adjust indices to account for vertex offset in batch
        if (call.indexType == EIT_16BIT) {
            const u16* sourceIndices = static_cast<const u16*>(call.indices);
            u16* destIndices = reinterpret_cast<u16*>(batch.indexData.data() + currentIndexSize);
            for (u32 i = 0; i < call.indexCount; i++) {
                destIndices[i] = sourceIndices[i] + vertexOffset;
            }
        } else if (call.indexType == EIT_32BIT) {
            const u32* sourceIndices = static_cast<const u32*>(call.indices);
            u32* destIndices = reinterpret_cast<u32*>(batch.indexData.data() + currentIndexSize);
            for (u32 i = 0; i < call.indexCount; i++) {
                destIndices[i] = sourceIndices[i] + vertexOffset;
            }
        }
    }

    // Add instance data
    InstanceData instanceData = {};
    instanceData.worldMatrix = call.worldMatrix;
    instanceData.normalMatrix = call.worldMatrix.getInverse().getTransposed();
    instanceData.tintColor = call.material.DiffuseColor;

    batch.instanceTransforms.push_back(call.worldMatrix);

    // Update batch totals
    batch.totalVertices += call.vertexCount;
    batch.totalIndices += call.indexCount;
    batch.instanceCount++;
    batch.needsUpload = true;
}

bool CWebGPURenderBatcher::materialsCompatible(const SMaterial& a, const SMaterial& b) const {
    // Check core compatibility
    if (a.MaterialType != b.MaterialType) return false;
    if (a.getTexture(0) != b.getTexture(0)) return false; // Primary texture must match

    // Allow some variation in colors and properties for instancing
    return true;
}

size_t CWebGPURenderBatcher::getVertexSize(E_VERTEX_TYPE type) const {
    switch (type) {
        case EVT_STANDARD:
            return sizeof(S3DVertex);
        case EVT_2TCOORDS:
            return sizeof(S3DVertex2TCoords);
        case EVT_TANGENTS:
            return sizeof(S3DVertexTangents);
        default:
            return sizeof(S3DVertex);
    }
}

size_t CWebGPURenderBatcher::getIndexSize(E_INDEX_TYPE type) const {
    switch (type) {
        case EIT_16BIT:
            return sizeof(u16);
        case EIT_32BIT:
            return sizeof(u32);
        default:
            return sizeof(u16);
    }
}

WGPUBuffer CWebGPURenderBatcher::createVertexBuffer(u32 size) {
    WGPUBufferDescriptor desc = {};
    desc.label = "Batched Vertex Buffer";
    desc.size = size;
    desc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    return wgpuDeviceCreateBuffer(device, &desc);
}

WGPUBuffer CWebGPURenderBatcher::createIndexBuffer(u32 size) {
    WGPUBufferDescriptor desc = {};
    desc.label = "Batched Index Buffer";
    desc.size = size;
    desc.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
    return wgpuDeviceCreateBuffer(device, &desc);
}

WGPUBuffer CWebGPURenderBatcher::createInstanceBuffer(u32 size) {
    WGPUBufferDescriptor desc = {};
    desc.label = "Batched Instance Buffer";
    desc.size = size;
    desc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    return wgpuDeviceCreateBuffer(device, &desc);
}

void CWebGPURenderBatcher::updateBatchBuffers(RenderBatch& batch) {
    // Create or update vertex buffer
    if (!batch.vertexBuffer || wgpuBufferGetSize(batch.vertexBuffer) < batch.vertexData.size()) {
        if (batch.vertexBuffer) wgpuBufferRelease(batch.vertexBuffer);
        batch.vertexBuffer = createVertexBuffer(static_cast<u32>(batch.vertexData.size()));
    }

    if (!batch.vertexData.empty()) {
        wgpuQueueWriteBuffer(queue, batch.vertexBuffer, 0,
                            batch.vertexData.data(), batch.vertexData.size());
    }

    // Create or update index buffer
    if (!batch.indexData.empty()) {
        if (!batch.indexBuffer || wgpuBufferGetSize(batch.indexBuffer) < batch.indexData.size()) {
            if (batch.indexBuffer) wgpuBufferRelease(batch.indexBuffer);
            batch.indexBuffer = createIndexBuffer(static_cast<u32>(batch.indexData.size()));
        }

        wgpuQueueWriteBuffer(queue, batch.indexBuffer, 0,
                            batch.indexData.data(), batch.indexData.size());
    }

    // Create instance buffer if needed
    if (instancingEnabled && batch.instanceCount > 1) {
        size_t instanceSize = batch.instanceCount * sizeof(InstanceData);
        if (!batch.instanceBuffer || wgpuBufferGetSize(batch.instanceBuffer) < instanceSize) {
            if (batch.instanceBuffer) wgpuBufferRelease(batch.instanceBuffer);
            batch.instanceBuffer = createInstanceBuffer(static_cast<u32>(instanceSize));
        }

        // Upload instance data
        std::vector<InstanceData> instanceData;
        instanceData.reserve(batch.instanceCount);
        for (const auto& transform : batch.instanceTransforms) {
            InstanceData data = {};
            data.worldMatrix = transform;
            data.normalMatrix = transform.getInverse().getTransposed();
            data.tintColor = batch.material.DiffuseColor;
            instanceData.push_back(data);
        }

        wgpuQueueWriteBuffer(queue, batch.instanceBuffer, 0,
                            instanceData.data(), instanceData.size() * sizeof(InstanceData));
    }
}

void CWebGPURenderBatcher::executeBatch(const RenderBatch& batch) {
    if (debugMode) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer),
                "🎯 Executing batch: %u vertices, %u indices, %u instances",
                batch.totalVertices, batch.totalIndices, batch.instanceCount);
        os::Printer::log(buffer, ELL_DEBUG);
    }

    // This would integrate with the main WebGPU driver's render pass
    // For now, we demonstrate the structure

    // Set vertex buffers
    // wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, batch.vertexBuffer, 0, WGPU_WHOLE_SIZE);

    // Set index buffer if present
    if (batch.indexBuffer) {
        // WGPUIndexFormat format = (batch.indexType == EIT_32BIT) ?
        //     WGPUIndexFormat_Uint32 : WGPUIndexFormat_Uint16;
        // wgpuRenderPassEncoderSetIndexBuffer(renderPass, batch.indexBuffer, format, 0, WGPU_WHOLE_SIZE);
    }

    // Set bind groups (materials, textures, etc.)
    if (batch.bindGroup) {
        // wgpuRenderPassEncoderSetBindGroup(renderPass, 0, batch.bindGroup, 0, nullptr);
    }

    // Execute draw call
    if (batch.indexBuffer) {
        // wgpuRenderPassEncoderDrawIndexed(renderPass, batch.totalIndices, batch.instanceCount, 0, 0, 0);
    } else {
        // wgpuRenderPassEncoderDraw(renderPass, batch.totalVertices, batch.instanceCount, 0, 0);
    }
}

void CWebGPURenderBatcher::executeInstancedBatch(const RenderBatch& batch) {
    if (debugMode) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer),
                "🚀 Executing instanced batch: %u instances, %u vertices per instance",
                batch.instanceCount, batch.totalVertices / batch.instanceCount);
        os::Printer::log(buffer, ELL_DEBUG);
    }

    // Similar to executeBatch but with instance buffer
    executeBatch(batch);
}

void CWebGPURenderBatcher::executeImmediateDrawCall(const DrawCall& call) {
    // Fallback for calls that can't be batched
    if (debugMode) {
        os::Printer::log("⚡ Executing immediate draw call (unbatched)", ELL_DEBUG);
    }

    // Would execute single draw call directly
    stats.totalBatches++; // Count as separate batch
}

u32 CWebGPURenderBatcher::hashMaterial(const SMaterial& material) const {
    u32 hash = 0;
    hash ^= static_cast<u32>(material.MaterialType);
    hash = (hash << 4) | (hash >> 28);

    if (material.getTexture(0)) {
        // Use texture pointer as hash component
        hash ^= reinterpret_cast<uintptr_t>(material.getTexture(0));
    }

    hash ^= material.DiffuseColor.color;
    return hash;
}

void CWebGPURenderBatcher::logBatchingStats() const {
    char buffer[512];
    snprintf(buffer, sizeof(buffer),
            "📊 Batching Stats - Calls: %u->%u (%.1f%% efficiency), "
            "Avg batch size: %u, Vertices: %u, Indices: %u",
            stats.totalDrawCalls, stats.totalBatches, stats.batchingEfficiency,
            stats.averageBatchSize, stats.verticesProcessed, stats.indicesProcessed);
    os::Printer::log(buffer, ELL_INFORMATION);
}

void CWebGPURenderBatcher::resetStats() {
    memset(&stats, 0, sizeof(stats));
}

} // namespace video
} // namespace irr

#endif // _IRR_COMPILE_WITH_WEBGPU_