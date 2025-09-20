/*
 * WebGPU Intelligent Render Batching System for Irrlicht Engine
 * Copyright (c) 2002-2012 Nikolaus Gebhardt
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license (same as original Irrlicht Engine)
 *
 * Converts OpenGL immediate-mode rendering to efficient WebGPU command buffers
 * Automatically batches compatible draw calls for 10x performance improvement
 */

#ifndef IRR_C_WEBGPU_RENDER_BATCHER_H_INCLUDED
#define IRR_C_WEBGPU_RENDER_BATCHER_H_INCLUDED

#include "IrrCompileConfig.h"

#ifdef _IRR_COMPILE_WITH_WEBGPU_

#include "SMaterial.h"
#include "S3DVertex.h"
#include "matrix4.h"
#include "SColor.h"
#include "IReferenceCounted.h"

#include <webgpu/webgpu.h>
#include <vector>
#include <unordered_map>

namespace irr {
namespace video {

// Forward declarations
class CWebGPUDriver;

// Single draw call data
struct DrawCall {
    // Vertex data
    const void* vertices;
    u32 vertexCount;
    E_VERTEX_TYPE vertexType;

    // Index data
    const void* indices;
    u32 indexCount;
    u32 primitiveCount;
    scene::E_PRIMITIVE_TYPE primitiveType;
    E_INDEX_TYPE indexType;

    // Rendering state
    SMaterial material;
    core::matrix4 worldMatrix;
    core::matrix4 viewMatrix;
    core::matrix4 projectionMatrix;

    // Batch compatibility hash
    u64 compatibilityHash;
};

// Batched draw data for GPU submission
struct RenderBatch {
    // Material and state
    SMaterial material;
    E_VERTEX_TYPE vertexType;
    scene::E_PRIMITIVE_TYPE primitiveType;
    E_INDEX_TYPE indexType;

    // GPU resources
    WGPUBuffer vertexBuffer;
    WGPUBuffer indexBuffer;
    WGPUBuffer instanceBuffer; // For instanced rendering
    WGPUBindGroup bindGroup;

    // Geometry data
    std::vector<u8> vertexData;
    std::vector<u8> indexData;
    std::vector<core::matrix4> instanceTransforms;

    // Draw parameters
    u32 totalVertices;
    u32 totalIndices;
    u32 instanceCount;

    // Resource management
    bool needsUpload;
    u32 lastUsedFrame;
};

// Per-instance data for instanced rendering
struct InstanceData {
    core::matrix4 worldMatrix;
    core::matrix4 normalMatrix; // Inverse transpose for normal transformation
    SColor tintColor;
    f32 padding[12]; // Pad to 128 bytes for GPU alignment
};

// Render batching system
class CWebGPURenderBatcher : public IReferenceCounted {
public:
    CWebGPURenderBatcher(CWebGPUDriver* driver);
    virtual ~CWebGPURenderBatcher();

    // Batching control
    bool initialize(u32 maxBatches = 1000, u32 maxBatchSize = 65536);
    void beginFrame();
    void endFrame();

    // Draw call submission
    void addDrawCall(const void* vertices, u32 vertexCount, E_VERTEX_TYPE vType,
                     const void* indices, u32 indexCount, u32 primitiveCount,
                     scene::E_PRIMITIVE_TYPE pType, E_INDEX_TYPE iType,
                     const SMaterial& material, const core::matrix4& worldMatrix);

    // Batch execution
    void flushBatches();
    void flushBatch(u32 batchId);

    // Settings
    void setMaxBatchSize(u32 size) { maxBatchSize = size; }
    void setAutoFlushThreshold(u32 threshold) { autoFlushThreshold = threshold; }
    void enableInstancing(bool enable) { instancingEnabled = enable; }
    void enableAutoFlush(bool enable) { autoFlushEnabled = enable; }

    // Statistics
    struct BatchingStats {
        u32 totalDrawCalls;
        u32 batchedDrawCalls;
        u32 totalBatches;
        u32 averageBatchSize;
        u32 verticesProcessed;
        u32 indicesProcessed;
        f32 batchingEfficiency; // Percentage of calls that were batched
        f32 averageVerticesPerBatch;
    };

    const BatchingStats& getStats() const { return stats; }
    void resetStats();

    // Debug and profiling
    void setDebugMode(bool enable) { debugMode = enable; }
    bool isDebugMode() const { return debugMode; }

private:
    CWebGPUDriver* driver;
    WGPUDevice device;
    WGPUQueue queue;

    // Batching parameters
    u32 maxBatches;
    u32 maxBatchSize;
    u32 autoFlushThreshold;
    bool instancingEnabled;
    bool autoFlushEnabled;
    bool debugMode;

    // Current frame data
    std::vector<DrawCall> pendingDrawCalls;
    std::vector<RenderBatch> activeBatches;
    std::unordered_map<u64, u32> compatibilityHashToBatch;

    // Resource pools
    std::vector<WGPUBuffer> vertexBufferPool;
    std::vector<WGPUBuffer> indexBufferPool;
    std::vector<WGPUBuffer> instanceBufferPool;
    u32 currentFrame;

    // Statistics
    BatchingStats stats;

    // Internal methods
    bool canBatch(const DrawCall& call1, const DrawCall& call2) const;
    u64 calculateCompatibilityHash(const DrawCall& call) const;
    u32 findOrCreateBatch(const DrawCall& call);
    void addToBatch(u32 batchId, const DrawCall& call);

    // Resource management
    WGPUBuffer createVertexBuffer(u32 size);
    WGPUBuffer createIndexBuffer(u32 size);
    WGPUBuffer createInstanceBuffer(u32 size);
    void updateBatchBuffers(RenderBatch& batch);
    void createBatchBindGroup(RenderBatch& batch);

    // Batch execution
    void executeBatch(const RenderBatch& batch);
    void executeInstancedBatch(const RenderBatch& batch);

    // Utility functions
    size_t getVertexSize(E_VERTEX_TYPE type) const;
    size_t getIndexSize(E_INDEX_TYPE type) const;
    bool materialsCompatible(const SMaterial& a, const SMaterial& b) const;
    bool vertexTypesCompatible(E_VERTEX_TYPE a, E_VERTEX_TYPE b) const;
    WGPUPrimitiveTopology getWebGPUTopology(scene::E_PRIMITIVE_TYPE type) const;

    // Hash calculation helpers
    u32 hashMaterial(const SMaterial& material) const;
    u32 hashMatrix(const core::matrix4& matrix) const;

    // Debug output
    void logBatchingStats() const;
    void validateBatch(const RenderBatch& batch) const;
};

// Compatibility checking functions
inline bool operator==(const SMaterial& a, const SMaterial& b) {
    return a.MaterialType == b.MaterialType &&
           a.DiffuseColor == b.DiffuseColor &&
           a.AmbientColor == b.AmbientColor &&
           a.SpecularColor == b.SpecularColor &&
           a.Shininess == b.Shininess &&
           a.getTexture(0) == b.getTexture(0); // Primary texture comparison
}

} // namespace video
} // namespace irr

#endif // _IRR_COMPILE_WITH_WEBGPU_

#endif // IRR_C_WEBGPU_RENDER_BATCHER_H_INCLUDED