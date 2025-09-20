/*
 * WebGPU Memory Pool Implementation for Irrlicht Engine
 * Copyright (c) 2002-2012 Nikolaus Gebhardt
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license (same as original Irrlicht Engine)
 */

#include "CWebGPUMemoryPool.h"
#include "CWebGPUDriver.h"
#include "os.h"
#include "matrix4.h"

#ifdef _IRR_COMPILE_WITH_WEBGPU_

#include <algorithm>
#include <cstring>

namespace irr {
namespace video {

//==============================================================================
// CWebGPUBufferPool Implementation
//==============================================================================

CWebGPUBufferPool::CWebGPUBufferPool(WGPUDevice device, BufferType type, u32 poolSize, u32 alignment)
    : device(device), type(type), megaBuffer(nullptr), mappedMemory(nullptr),
      poolSize(poolSize), usedSize(0), defaultAlignment(alignment), currentFrame(0) {

    memset(&stats, 0, sizeof(stats));

    if (!createMegaBuffer()) {
        os::Printer::log("❌ Failed to create WebGPU buffer pool", ELL_ERROR);
    } else {
        // Initialize with one large free block
        FreeBlock initialBlock = { 0, poolSize, 0 };
        freeBlocks.push_back(initialBlock);

        char buffer[256];
        snprintf(buffer, sizeof(buffer),
                "✅ Created WebGPU buffer pool: %s, %.1f MB",
                getBufferLabel(), poolSize / (1024.0f * 1024.0f));
        os::Printer::log(buffer, ELL_INFORMATION);
    }
}

CWebGPUBufferPool::~CWebGPUBufferPool() {
    if (megaBuffer) {
        wgpuBufferRelease(megaBuffer);
    }
}

bool CWebGPUBufferPool::createMegaBuffer() {
    if (!device) return false;

    WGPUBufferDescriptor desc = {};
    desc.label = getBufferLabel();
    desc.size = poolSize;
    desc.usage = getBufferUsage();
    desc.mappedAtCreation = false; // We'll map when needed

    megaBuffer = wgpuDeviceCreateBuffer(device, &desc);
    return megaBuffer != nullptr;
}

WGPUBufferUsageFlags CWebGPUBufferPool::getBufferUsage() const {
    switch (type) {
        case BUFFER_TYPE_VERTEX:
            return WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
        case BUFFER_TYPE_INDEX:
            return WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
        case BUFFER_TYPE_UNIFORM:
            return WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
        case BUFFER_TYPE_STORAGE:
            return WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
        case BUFFER_TYPE_INDIRECT:
            return WGPUBufferUsage_Indirect | WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst;
        default:
            return WGPUBufferUsage_CopyDst;
    }
}

const char* CWebGPUBufferPool::getBufferLabel() const {
    switch (type) {
        case BUFFER_TYPE_VERTEX: return "Vertex Memory Pool";
        case BUFFER_TYPE_INDEX: return "Index Memory Pool";
        case BUFFER_TYPE_UNIFORM: return "Uniform Memory Pool";
        case BUFFER_TYPE_STORAGE: return "Storage Memory Pool";
        case BUFFER_TYPE_INDIRECT: return "Indirect Memory Pool";
        default: return "Unknown Memory Pool";
    }
}

MemoryAllocation CWebGPUBufferPool::allocate(u32 size, u32 alignment) {
    if (alignment == 0) alignment = defaultAlignment;

    u32 alignedSize = alignSize(size, alignment);
    u32 blockIndex = findFreeBlock(alignedSize, alignment);

    if (blockIndex == static_cast<u32>(-1)) {
        stats.failedAllocations++;
        return {}; // Allocation failed
    }

    FreeBlock& block = freeBlocks[blockIndex];
    u32 offset = alignSize(block.offset, alignment);

    // Create allocation
    MemoryAllocation allocation = {};
    allocation.buffer = megaBuffer;
    allocation.offset = offset;
    allocation.size = size;
    allocation.alignment = alignment;
    allocation.mappedPtr = nullptr; // Will be mapped on demand
    allocation.frameAllocated = currentFrame;
    allocation.frameReleased = 0;
    allocation.inUse = true;

    // Split the block if necessary
    u32 actualUsedSize = alignedSize;
    if (block.size > actualUsedSize) {
        // Create new free block from remainder
        FreeBlock remainderBlock = {};
        remainderBlock.offset = offset + actualUsedSize;
        remainderBlock.size = block.size - actualUsedSize;
        remainderBlock.frameFreed = currentFrame;
        freeBlocks.push_back(remainderBlock);
    }

    // Remove or update the used block
    freeBlocks.erase(freeBlocks.begin() + blockIndex);

    usedSize += actualUsedSize;
    stats.totalAllocations++;
    stats.activeAllocations++;

    activeAllocations.push_back(allocation);

    return allocation;
}

void CWebGPUBufferPool::deallocate(const MemoryAllocation& allocation) {
    if (!allocation.inUse || allocation.buffer != megaBuffer) {
        return; // Invalid allocation
    }

    // Find and remove from active allocations
    auto it = std::find_if(activeAllocations.begin(), activeAllocations.end(),
                          [&allocation](const MemoryAllocation& alloc) {
                              return alloc.offset == allocation.offset && alloc.size == allocation.size;
                          });

    if (it != activeAllocations.end()) {
        u32 actualSize = alignSize(allocation.size, allocation.alignment);

        // Add back to free blocks
        FreeBlock freeBlock = {};
        freeBlock.offset = allocation.offset;
        freeBlock.size = actualSize;
        freeBlock.frameFreed = currentFrame;
        freeBlocks.push_back(freeBlock);

        usedSize -= actualSize;
        stats.activeAllocations--;

        activeAllocations.erase(it);

        // Coalesce adjacent free blocks
        coalesceFreeBlocks();
    }
}

void CWebGPUBufferPool::advanceFrame() {
    currentFrame++;

    // Clean up old allocations (after 2 frames to ensure GPU is done)
    for (auto it = activeAllocations.begin(); it != activeAllocations.end();) {
        if (!it->inUse && (currentFrame - it->frameReleased) >= 2) {
            deallocate(*it);
            it = activeAllocations.erase(it);
        } else {
            ++it;
        }
    }

    updateStats();
}

u32 CWebGPUBufferPool::findFreeBlock(u32 size, u32 alignment) {
    u32 bestIndex = static_cast<u32>(-1);
    u32 bestSize = static_cast<u32>(-1);

    for (u32 i = 0; i < freeBlocks.size(); i++) {
        const FreeBlock& block = freeBlocks[i];
        u32 alignedOffset = alignSize(block.offset, alignment);
        u32 alignedSize = alignedOffset - block.offset + size;

        if (block.size >= alignedSize && block.size < bestSize) {
            bestIndex = i;
            bestSize = block.size;
        }
    }

    return bestIndex;
}

void CWebGPUBufferPool::coalesceFreeBlocks() {
    if (freeBlocks.size() < 2) return;

    // Sort by offset
    std::sort(freeBlocks.begin(), freeBlocks.end(),
             [](const FreeBlock& a, const FreeBlock& b) {
                 return a.offset < b.offset;
             });

    // Coalesce adjacent blocks
    for (auto it = freeBlocks.begin(); it != freeBlocks.end() - 1;) {
        if (it->offset + it->size == (it + 1)->offset) {
            // Merge with next block
            it->size += (it + 1)->size;
            it = freeBlocks.erase(it + 1) - 1;
        } else {
            ++it;
        }
    }
}

void CWebGPUBufferPool::updateStats() {
    stats.fragmentedSize = 0;
    stats.largestFreeBlock = 0;

    for (const auto& block : freeBlocks) {
        stats.fragmentedSize += block.size;
        if (block.size > stats.largestFreeBlock) {
            stats.largestFreeBlock = block.size;
        }
    }

    if (stats.totalAllocations > 0) {
        stats.averageAllocationSize = (f32)usedSize / stats.activeAllocations;
    }

    if (poolSize > 0) {
        stats.fragmentation = (f32)(stats.fragmentedSize - stats.largestFreeBlock) / poolSize * 100.0f;
    }
}

u32 CWebGPUBufferPool::alignSize(u32 size, u32 alignment) const {
    return (size + alignment - 1) & ~(alignment - 1);
}

bool CWebGPUBufferPool::isAligned(u32 offset, u32 alignment) const {
    return (offset & (alignment - 1)) == 0;
}

//==============================================================================
// CWebGPUMemoryPool Implementation
//==============================================================================

CWebGPUMemoryPool::CWebGPUMemoryPool(CWebGPUDriver* driver)
    : driver(driver), device(nullptr), queue(nullptr), initialized(false),
      debugMode(false), currentFrame(0),
      vertexPool(nullptr), indexPool(nullptr), uniformPool(nullptr),
      storagePool(nullptr), indirectPool(nullptr),
      transformRing(nullptr, 1024), materialRing(nullptr, 512), lightRing(nullptr, 64) {

    memset(&stats, 0, sizeof(stats));

    if (driver) {
        device = driver->getWebGPUDevice();
        if (device) {
            queue = wgpuDeviceGetQueue(device);

            // Initialize ring buffers with device
            transformRing = TransformRingBuffer(device, 1024); // 1024 transforms per frame
            materialRing = MaterialRingBuffer(device, 512 * 16); // 512 materials, 16 floats each
            lightRing = LightRingBuffer(device, 64 * 16); // 64 lights, 16 floats each
        }
    }
}

CWebGPUMemoryPool::~CWebGPUMemoryPool() {
    destroyPools();
}

bool CWebGPUMemoryPool::initialize() {
    if (initialized) return true;

    if (!device || !queue) {
        os::Printer::log("❌ CWebGPUMemoryPool: No WebGPU device available", ELL_ERROR);
        return false;
    }

    if (!createPools()) {
        os::Printer::log("❌ CWebGPUMemoryPool: Failed to create buffer pools", ELL_ERROR);
        return false;
    }

    initialized = true;

    char buffer[512];
    snprintf(buffer, sizeof(buffer),
            "✅ WebGPU Memory Pool initialized - Total: %.1f MB "
            "(Vertex: %.1f MB, Index: %.1f MB, Uniform: %.1f MB, Storage: %.1f MB, Indirect: %.1f MB)",
            (config.vertexPoolSize + config.indexPoolSize + config.uniformPoolSize +
             config.storagePoolSize + config.indirectPoolSize) / (1024.0f * 1024.0f),
            config.vertexPoolSize / (1024.0f * 1024.0f),
            config.indexPoolSize / (1024.0f * 1024.0f),
            config.uniformPoolSize / (1024.0f * 1024.0f),
            config.storagePoolSize / (1024.0f * 1024.0f),
            config.indirectPoolSize / (1024.0f * 1024.0f));
    os::Printer::log(buffer, ELL_INFORMATION);

    return true;
}

bool CWebGPUMemoryPool::createPools() {
    vertexPool = new CWebGPUBufferPool(device, CWebGPUBufferPool::BUFFER_TYPE_VERTEX,
                                      config.vertexPoolSize, 16);
    if (!vertexPool->getTotalSize()) return false;

    indexPool = new CWebGPUBufferPool(device, CWebGPUBufferPool::BUFFER_TYPE_INDEX,
                                     config.indexPoolSize, 4);
    if (!indexPool->getTotalSize()) return false;

    uniformPool = new CWebGPUBufferPool(device, CWebGPUBufferPool::BUFFER_TYPE_UNIFORM,
                                       config.uniformPoolSize, 256);
    if (!uniformPool->getTotalSize()) return false;

    storagePool = new CWebGPUBufferPool(device, CWebGPUBufferPool::BUFFER_TYPE_STORAGE,
                                       config.storagePoolSize, 16);
    if (!storagePool->getTotalSize()) return false;

    indirectPool = new CWebGPUBufferPool(device, CWebGPUBufferPool::BUFFER_TYPE_INDIRECT,
                                        config.indirectPoolSize, 4);
    if (!indirectPool->getTotalSize()) return false;

    return true;
}

void CWebGPUMemoryPool::destroyPools() {
    delete vertexPool;
    delete indexPool;
    delete uniformPool;
    delete storagePool;
    delete indirectPool;

    vertexPool = nullptr;
    indexPool = nullptr;
    uniformPool = nullptr;
    storagePool = nullptr;
    indirectPool = nullptr;
}

MemoryAllocation CWebGPUMemoryPool::allocateVertexMemory(u32 size, u32 alignment) {
    if (!initialized || !vertexPool) return {};
    auto allocation = vertexPool->allocate(size, alignment);
    if (allocation.buffer) {
        bufferToPool[allocation.buffer] = vertexPool;
        activeAllocations.push_back(allocation);
        stats.totalAllocations++;
    }
    return allocation;
}

MemoryAllocation CWebGPUMemoryPool::allocateIndexMemory(u32 size, u32 alignment) {
    if (!initialized || !indexPool) return {};
    auto allocation = indexPool->allocate(size, alignment);
    if (allocation.buffer) {
        bufferToPool[allocation.buffer] = indexPool;
        activeAllocations.push_back(allocation);
        stats.totalAllocations++;
    }
    return allocation;
}

MemoryAllocation CWebGPUMemoryPool::allocateUniformMemory(u32 size, u32 alignment) {
    if (!initialized || !uniformPool) return {};
    auto allocation = uniformPool->allocate(size, alignment);
    if (allocation.buffer) {
        bufferToPool[allocation.buffer] = uniformPool;
        activeAllocations.push_back(allocation);
        stats.totalAllocations++;
    }
    return allocation;
}

MemoryAllocation CWebGPUMemoryPool::allocateStorageMemory(u32 size, u32 alignment) {
    if (!initialized || !storagePool) return {};
    auto allocation = storagePool->allocate(size, alignment);
    if (allocation.buffer) {
        bufferToPool[allocation.buffer] = storagePool;
        activeAllocations.push_back(allocation);
        stats.totalAllocations++;
    }
    return allocation;
}

MemoryAllocation CWebGPUMemoryPool::allocateIndirectMemory(u32 size, u32 alignment) {
    if (!initialized || !indirectPool) return {};
    auto allocation = indirectPool->allocate(size, alignment);
    if (allocation.buffer) {
        bufferToPool[allocation.buffer] = indirectPool;
        activeAllocations.push_back(allocation);
        stats.totalAllocations++;
    }
    return allocation;
}

void CWebGPUMemoryPool::deallocateMemory(const MemoryAllocation& allocation) {
    auto it = bufferToPool.find(allocation.buffer);
    if (it != bufferToPool.end()) {
        it->second->deallocate(allocation);
        bufferToPool.erase(it);

        // Remove from active allocations
        auto allocIt = std::find_if(activeAllocations.begin(), activeAllocations.end(),
                                   [&allocation](const MemoryAllocation& alloc) {
                                       return alloc.buffer == allocation.buffer &&
                                              alloc.offset == allocation.offset;
                                   });
        if (allocIt != activeAllocations.end()) {
            activeAllocations.erase(allocIt);
        }
    }
}

void CWebGPUMemoryPool::beginFrame() {
    // Prepare ring buffers for new frame data
    transformRing.advanceFrame();
    materialRing.advanceFrame();
    lightRing.advanceFrame();
}

void CWebGPUMemoryPool::endFrame() {
    // Upload ring buffer data to GPU
    if (queue) {
        transformRing.uploadCurrentFrame(queue);
        materialRing.uploadCurrentFrame(queue);
        lightRing.uploadCurrentFrame(queue);
    }

    updateStats();

    if (debugMode) {
        logMemoryUsage();
    }
}

void CWebGPUMemoryPool::advanceFrame() {
    currentFrame++;
    stats.frameAdvances++;

    // Advance all buffer pools
    if (vertexPool) vertexPool->advanceFrame();
    if (indexPool) indexPool->advanceFrame();
    if (uniformPool) uniformPool->advanceFrame();
    if (storagePool) storagePool->advanceFrame();
    if (indirectPool) indirectPool->advanceFrame();

    // Clean up old allocations
    cleanupOldAllocations();

    // Periodic compaction
    if (currentFrame % 60 == 0) { // Every 60 frames (~1 second at 60 FPS)
        compactPools();
    }
}

void CWebGPUMemoryPool::updateStats() {
    if (!initialized) return;

    stats.totalPoolSize = config.vertexPoolSize + config.indexPoolSize +
                         config.uniformPoolSize + config.storagePoolSize + config.indirectPoolSize;

    stats.totalUsedSize = 0;
    f32 totalUtilization = 0.0f;
    f32 totalFragmentation = 0.0f;
    u32 poolCount = 0;

    if (vertexPool) {
        stats.totalUsedSize += vertexPool->getUsedSize();
        totalUtilization += vertexPool->getUtilization();
        totalFragmentation += vertexPool->getStats().fragmentation;
        poolCount++;
    }
    if (indexPool) {
        stats.totalUsedSize += indexPool->getUsedSize();
        totalUtilization += indexPool->getUtilization();
        totalFragmentation += indexPool->getStats().fragmentation;
        poolCount++;
    }
    if (uniformPool) {
        stats.totalUsedSize += uniformPool->getUsedSize();
        totalUtilization += uniformPool->getUtilization();
        totalFragmentation += uniformPool->getStats().fragmentation;
        poolCount++;
    }
    if (storagePool) {
        stats.totalUsedSize += storagePool->getUsedSize();
        totalUtilization += storagePool->getUtilization();
        totalFragmentation += storagePool->getStats().fragmentation;
        poolCount++;
    }
    if (indirectPool) {
        stats.totalUsedSize += indirectPool->getUsedSize();
        totalUtilization += indirectPool->getUtilization();
        totalFragmentation += indirectPool->getStats().fragmentation;
        poolCount++;
    }

    stats.totalFreeSize = stats.totalPoolSize - stats.totalUsedSize;

    if (poolCount > 0) {
        stats.overallUtilization = totalUtilization / poolCount;
        stats.averageFragmentation = totalFragmentation / poolCount;
    }

    // Update failure statistics
    stats.failedAllocations = 0;
    if (vertexPool) stats.failedAllocations += vertexPool->getStats().failedAllocations;
    if (indexPool) stats.failedAllocations += indexPool->getStats().failedAllocations;
    if (uniformPool) stats.failedAllocations += uniformPool->getStats().failedAllocations;
    if (storagePool) stats.failedAllocations += storagePool->getStats().failedAllocations;
    if (indirectPool) stats.failedAllocations += indirectPool->getStats().failedAllocations;
}

void CWebGPUMemoryPool::logMemoryUsage() const {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer),
            "📊 Memory Pool Stats - Total: %.1f/%.1f MB (%.1f%% used), "
            "Fragmentation: %.1f%%, Failed allocations: %u, Frame: %u",
            stats.totalUsedSize / (1024.0f * 1024.0f),
            stats.totalPoolSize / (1024.0f * 1024.0f),
            stats.overallUtilization * 100.0f,
            stats.averageFragmentation,
            stats.failedAllocations,
            currentFrame);
    os::Printer::log(buffer, ELL_INFORMATION);
}

void CWebGPUMemoryPool::compactPools() {
    // This could trigger defragmentation, but for now we just log
    if (debugMode && stats.averageFragmentation > 25.0f) {
        os::Printer::log("⚠️ Memory fragmentation > 25%, consider optimization", ELL_WARNING);
    }
}

void CWebGPUMemoryPool::cleanupOldAllocations() {
    // Remove allocations that haven't been used recently
    activeAllocations.erase(
        std::remove_if(activeAllocations.begin(), activeAllocations.end(),
                      [this](const MemoryAllocation& alloc) {
                          return !alloc.inUse && (currentFrame - alloc.frameReleased) >= 3;
                      }),
        activeAllocations.end());
}

void CWebGPUMemoryPool::validatePools() const {
    if (!initialized) return;

    if (debugMode) {
        os::Printer::log("🔍 Validating memory pools...", ELL_DEBUG);

        if (vertexPool && vertexPool->getUtilization() > 0.95f) {
            os::Printer::log("⚠️ Vertex pool nearly full", ELL_WARNING);
        }
        if (indexPool && indexPool->getUtilization() > 0.95f) {
            os::Printer::log("⚠️ Index pool nearly full", ELL_WARNING);
        }
        if (uniformPool && uniformPool->getUtilization() > 0.95f) {
            os::Printer::log("⚠️ Uniform pool nearly full", ELL_WARNING);
        }
        if (storagePool && storagePool->getUtilization() > 0.95f) {
            os::Printer::log("⚠️ Storage pool nearly full", ELL_WARNING);
        }
        if (indirectPool && indirectPool->getUtilization() > 0.95f) {
            os::Printer::log("⚠️ Indirect pool nearly full", ELL_WARNING);
        }

        os::Printer::log("✅ Memory pool validation complete", ELL_DEBUG);
    }
}

} // namespace video
} // namespace irr

#endif // _IRR_COMPILE_WITH_WEBGPU_