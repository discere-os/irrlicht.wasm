/*
 * WebGPU Compute-based Culling System for Irrlicht Engine
 * Copyright (c) 2002-2012 Nikolaus Gebhardt
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license (same as original Irrlicht Engine)
 *
 * GPU-driven frustum culling and indirect rendering for massive scene complexity
 */

#ifndef IRR_C_WEBGPU_COMPUTE_CULLING_H_INCLUDED
#define IRR_C_WEBGPU_COMPUTE_CULLING_H_INCLUDED

#include "IrrCompileConfig.h"

#ifdef _IRR_COMPILE_WITH_WEBGPU_

#include "vector3d.h"
#include "matrix4.h"
#include "aabbox3d.h"
#include "SMaterial.h"
#include "S3DVertex.h"

#include <webgpu/webgpu.h>
#include <vector>

namespace irr {
namespace video {

// Forward declarations
class CWebGPUDriver;

// GPU-side object instance data
struct ObjectInstance {
    core::matrix4 worldMatrix;        // 64 bytes: World transformation
    core::vector3df boundingSphere;    // 12 bytes: Center (xyz)
    f32 boundingRadius;               // 4 bytes: Radius (w)
    u32 materialIndex;                // 4 bytes: Material ID
    u32 vertexOffset;                 // 4 bytes: First vertex in mega buffer
    u32 indexOffset;                  // 4 bytes: First index in mega buffer
    u32 indexCount;                   // 4 bytes: Number of indices to draw
    u32 flags;                        // 4 bytes: Visibility flags, LOD level
    u32 padding[3];                   // 12 bytes: Pad to 128 bytes for GPU alignment
};

// GPU culling data
struct CullingData {
    core::vector3df cameraPosition;    // 12 bytes
    f32 maxRenderDistance;            // 4 bytes
    core::vector3df frustumPlanes[6]; // 72 bytes: 6 planes * 12 bytes each
    f32 frustumDistances[6];          // 24 bytes: Distance component for each plane
    u32 objectCount;                  // 4 bytes
    u32 enableDistanceCulling;        // 4 bytes: Boolean flag
    u32 enableOcclusionCulling;       // 4 bytes: Boolean flag
    u32 lodBias;                      // 4 bytes: LOD scaling factor
    u32 padding[12];                  // 48 bytes: Pad to 256 bytes
};

// Indirect draw command (matches WebGPU spec)
struct IndirectDrawCommand {
    u32 vertexCount;
    u32 instanceCount;
    u32 firstVertex;
    u32 firstInstance;
};

struct IndirectDrawIndexedCommand {
    u32 indexCount;
    u32 instanceCount;
    u32 firstIndex;
    s32 baseVertex;
    u32 firstInstance;
};

// GPU-driven culling and rendering system
class CWebGPUComputeCulling {
public:
    CWebGPUComputeCulling(CWebGPUDriver* driver);
    ~CWebGPUComputeCulling();

    // Initialization
    bool initialize(u32 maxObjects = 100000);
    bool isInitialized() const { return initialized; }

    // Object management
    u32 addObject(const core::matrix4& worldMatrix,
                  const core::aabbox3df& boundingBox,
                  u32 materialIndex,
                  u32 vertexOffset,
                  u32 indexOffset,
                  u32 indexCount,
                  u32 flags = 0);

    void updateObject(u32 objectId,
                      const core::matrix4& worldMatrix,
                      const core::aabbox3df& boundingBox);

    void removeObject(u32 objectId);
    void clearObjects();

    // Culling setup
    void setCameraParameters(const core::vector3df& position,
                           const core::matrix4& viewProjection);

    void setRenderDistance(f32 maxDistance) { cullingData.maxRenderDistance = maxDistance; }
    void enableDistanceCulling(bool enable) { cullingData.enableDistanceCulling = enable ? 1 : 0; }
    void enableOcclusionCulling(bool enable) { cullingData.enableOcclusionCulling = enable ? 1 : 0; }
    void setLODBias(f32 bias) { cullingData.lodBias = static_cast<u32>(bias * 256.0f); }

    // Culling execution
    bool performCulling();
    u32 getVisibleObjectCount() const { return visibleCount; }
    const u32* getVisibilityResults() const;

    // Indirect rendering
    bool setupIndirectRendering();
    void executeIndirectDraw();

    // Statistics
    struct CullingStats {
        u32 totalObjects;
        u32 visibleObjects;
        u32 frustumCulled;
        u32 distanceCulled;
        u32 occlusionCulled;
        f32 cullingTimeMs;
    };

    const CullingStats& getStats() const { return stats; }

private:
    CWebGPUDriver* driver;
    bool initialized;

    // GPU resources
    WGPUDevice device;
    WGPUQueue queue;

    // Compute pipeline for culling
    WGPUComputePipeline cullingPipeline;
    WGPUBindGroupLayout cullingBindGroupLayout;
    WGPUBindGroup cullingBindGroup;

    // GPU buffers
    WGPUBuffer objectInstanceBuffer;   // Input: Object instances
    WGPUBuffer cullingDataBuffer;      // Input: Camera and culling parameters
    WGPUBuffer visibilityResultBuffer; // Output: Per-object visibility flags
    WGPUBuffer indirectCommandBuffer;  // Output: Indirect draw commands
    WGPUBuffer readbackBuffer;         // CPU readback for statistics

    // CPU data
    std::vector<ObjectInstance> objects;
    CullingData cullingData;
    CullingStats stats;
    u32 maxObjects;
    u32 visibleCount;

    // Internal methods
    bool createComputePipeline();
    bool createBuffers();
    bool createBindGroups();
    void updateBuffers();

    // Shader source
    const char* getCullingShaderSource();

    // Frustum extraction from view-projection matrix
    void extractFrustumPlanes(const core::matrix4& viewProjection);

    // Bounding sphere calculation from AABB
    void calculateBoundingSphere(const core::aabbox3df& box,
                               core::vector3df& center,
                               f32& radius);
};

} // namespace video
} // namespace irr

#endif // _IRR_COMPILE_WITH_WEBGPU_

#endif // IRR_C_WEBGPU_COMPUTE_CULLING_H_INCLUDED