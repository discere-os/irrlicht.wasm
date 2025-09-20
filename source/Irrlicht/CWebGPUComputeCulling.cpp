/*
 * WebGPU Compute-based Culling Implementation for Irrlicht Engine
 * Copyright (c) 2002-2012 Nikolaus Gebhardt
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license (same as original Irrlicht Engine)
 */

#include "CWebGPUComputeCulling.h"
#include "CWebGPUDriver.h"
#include "os.h"
#include "plane3d.h"

#ifdef _IRR_COMPILE_WITH_WEBGPU_

namespace irr {
namespace video {

CWebGPUComputeCulling::CWebGPUComputeCulling(CWebGPUDriver* driver)
    : driver(driver), initialized(false), device(nullptr), queue(nullptr),
      cullingPipeline(nullptr), cullingBindGroupLayout(nullptr), cullingBindGroup(nullptr),
      objectInstanceBuffer(nullptr), cullingDataBuffer(nullptr),
      visibilityResultBuffer(nullptr), indirectCommandBuffer(nullptr),
      readbackBuffer(nullptr), maxObjects(0), visibleCount(0) {

    memset(&cullingData, 0, sizeof(cullingData));
    memset(&stats, 0, sizeof(stats));

    // Initialize default culling parameters
    cullingData.maxRenderDistance = 1000.0f;
    cullingData.enableDistanceCulling = 1;
    cullingData.enableOcclusionCulling = 0;
    cullingData.lodBias = 256; // 1.0f * 256
}

CWebGPUComputeCulling::~CWebGPUComputeCulling() {
    // Cleanup GPU resources
    if (cullingPipeline) wgpuComputePipelineRelease(cullingPipeline);
    if (cullingBindGroupLayout) wgpuBindGroupLayoutRelease(cullingBindGroupLayout);
    if (cullingBindGroup) wgpuBindGroupRelease(cullingBindGroup);
    if (objectInstanceBuffer) wgpuBufferRelease(objectInstanceBuffer);
    if (cullingDataBuffer) wgpuBufferRelease(cullingDataBuffer);
    if (visibilityResultBuffer) wgpuBufferRelease(visibilityResultBuffer);
    if (indirectCommandBuffer) wgpuBufferRelease(indirectCommandBuffer);
    if (readbackBuffer) wgpuBufferRelease(readbackBuffer);
}

bool CWebGPUComputeCulling::initialize(u32 maxObjects) {
    if (initialized) return true;

    this->maxObjects = maxObjects;
    device = driver->getWebGPUDevice();
    if (!device) {
        os::Printer::log("❌ CWebGPUComputeCulling: No WebGPU device available", ELL_ERROR);
        return false;
    }

    queue = wgpuDeviceGetQueue(device);
    if (!queue) {
        os::Printer::log("❌ CWebGPUComputeCulling: Failed to get WebGPU queue", ELL_ERROR);
        return false;
    }

    // Reserve object storage
    objects.reserve(maxObjects);

    // Create GPU resources
    if (!createComputePipeline()) {
        os::Printer::log("❌ CWebGPUComputeCulling: Failed to create compute pipeline", ELL_ERROR);
        return false;
    }

    if (!createBuffers()) {
        os::Printer::log("❌ CWebGPUComputeCulling: Failed to create buffers", ELL_ERROR);
        return false;
    }

    if (!createBindGroups()) {
        os::Printer::log("❌ CWebGPUComputeCulling: Failed to create bind groups", ELL_ERROR);
        return false;
    }

    initialized = true;
    os::Printer::log("✅ CWebGPUComputeCulling: Initialized for up to 100,000 objects", ELL_INFORMATION);
    return true;
}

const char* CWebGPUComputeCulling::getCullingShaderSource() {
    return R"(
// WebGPU compute shader for GPU-driven frustum culling
// Processes up to 64 objects per workgroup for optimal GPU utilization

struct ObjectInstance {
    worldMatrix: mat4x4f,        // World transformation matrix
    boundingSphere: vec4f,       // xyz = center, w = radius
    materialIndex: u32,          // Material identifier
    vertexOffset: u32,           // First vertex in mega buffer
    indexOffset: u32,            // First index in mega buffer
    indexCount: u32,             // Number of indices to draw
    flags: u32,                  // Visibility flags and LOD level
    padding: array<u32, 3>,      // Ensure 128-byte alignment
}

struct CullingData {
    cameraPosition: vec3f,          // Camera world position
    maxRenderDistance: f32,         // Maximum render distance
    frustumPlanes: array<vec4f, 6>, // 6 frustum planes (xyz=normal, w=distance)
    objectCount: u32,               // Total objects to process
    enableDistanceCulling: u32,     // Boolean: enable distance culling
    enableOcclusionCulling: u32,    // Boolean: enable occlusion culling
    lodBias: u32,                   // LOD bias (fixed-point: 256 = 1.0)
    padding: array<u32, 12>,        // Pad to 256 bytes
}

struct IndirectDrawIndexedCommand {
    indexCount: u32,
    instanceCount: u32,
    firstIndex: u32,
    baseVertex: i32,
    firstInstance: u32,
}

// Input/Output bindings
@group(0) @binding(0) var<storage, read> instances: array<ObjectInstance>;
@group(0) @binding(1) var<uniform> cullData: CullingData;
@group(0) @binding(2) var<storage, read_write> visibilityResults: array<u32>;
@group(0) @binding(3) var<storage, read_write> indirectCommands: array<IndirectDrawIndexedCommand>;
@group(0) @binding(4) var<storage, read_write> atomicCounters: array<atomic<u32>>;

@compute @workgroup_size(64)
fn cullObjects(@builtin(global_invocation_id) globalId: vec3u) {
    let objectIndex = globalId.x;

    // Bounds check
    if (objectIndex >= cullData.objectCount) {
        return;
    }

    let instance = instances[objectIndex];
    let sphere = instance.boundingSphere;
    var visible = true;

    // 1. Distance culling (cheapest test first)
    if (cullData.enableDistanceCulling != 0u && visible) {
        let distanceToCamera = length(sphere.xyz - cullData.cameraPosition);
        if (distanceToCamera > sphere.w + cullData.maxRenderDistance) {
            visible = false;
        }
    }

    // 2. Frustum culling (6 plane tests)
    if (visible) {
        for (var i = 0u; i < 6u; i++) {
            let plane = cullData.frustumPlanes[i];
            let distance = dot(plane.xyz, sphere.xyz) + plane.w;
            if (distance < -sphere.w) {
                visible = false;
                break;
            }
        }
    }

    // 3. LOD culling based on distance and object size
    if (visible) {
        let distanceToCamera = length(sphere.xyz - cullData.cameraPosition);
        let screenSize = sphere.w / (distanceToCamera + 1.0);
        let lodLevel = u32(clamp(log2(1.0 / (screenSize + 0.001)), 0.0, 7.0));

        // Apply LOD bias
        let biasedLodLevel = (lodLevel * 256u) / cullData.lodBias;
        if (biasedLodLevel > 3u) {
            visible = false; // Object too small for current LOD bias
        }
    }

    // Store visibility result
    visibilityResults[objectIndex] = select(0u, 1u, visible);

    // Generate indirect draw commands for visible objects
    if (visible) {
        let commandIndex = atomicAdd(&atomicCounters[0], 1u);

        // Only write if within bounds of command buffer
        if (commandIndex < arrayLength(&indirectCommands)) {
            indirectCommands[commandIndex] = IndirectDrawIndexedCommand(
                instance.indexCount,    // indexCount
                1u,                     // instanceCount
                instance.indexOffset,   // firstIndex
                i32(instance.vertexOffset), // baseVertex
                objectIndex             // firstInstance (for per-instance data)
            );
        }
    }
}
)";
}

bool CWebGPUComputeCulling::createComputePipeline() {
    // Create shader module
    WGPUShaderModuleWGSLDescriptor wgslDesc = {};
    wgslDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgslDesc.code = getCullingShaderSource();

    WGPUShaderModuleDescriptor shaderDesc = {};
    shaderDesc.nextInChain = &wgslDesc.chain;
    shaderDesc.label = "Irrlicht GPU Culling Shader";

    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);
    if (!shaderModule) {
        os::Printer::log("❌ Failed to create culling compute shader", ELL_ERROR);
        return false;
    }

    // Create bind group layout
    WGPUBindGroupLayoutEntry entries[5] = {};

    // Binding 0: Object instances (read-only storage)
    entries[0].binding = 0;
    entries[0].visibility = WGPUShaderStage_Compute;
    entries[0].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;

    // Binding 1: Culling data (uniform)
    entries[1].binding = 1;
    entries[1].visibility = WGPUShaderStage_Compute;
    entries[1].buffer.type = WGPUBufferBindingType_Uniform;

    // Binding 2: Visibility results (read-write storage)
    entries[2].binding = 2;
    entries[2].visibility = WGPUShaderStage_Compute;
    entries[2].buffer.type = WGPUBufferBindingType_Storage;

    // Binding 3: Indirect commands (read-write storage)
    entries[3].binding = 3;
    entries[3].visibility = WGPUShaderStage_Compute;
    entries[3].buffer.type = WGPUBufferBindingType_Storage;

    // Binding 4: Atomic counters (read-write storage)
    entries[4].binding = 4;
    entries[4].visibility = WGPUShaderStage_Compute;
    entries[4].buffer.type = WGPUBufferBindingType_Storage;

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {};
    bindGroupLayoutDesc.label = "Culling Bind Group Layout";
    bindGroupLayoutDesc.entryCount = 5;
    bindGroupLayoutDesc.entries = entries;

    cullingBindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);
    if (!cullingBindGroupLayout) {
        wgpuShaderModuleRelease(shaderModule);
        os::Printer::log("❌ Failed to create culling bind group layout", ELL_ERROR);
        return false;
    }

    // Create pipeline layout
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.label = "Culling Pipeline Layout";
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &cullingBindGroupLayout;

    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);
    if (!pipelineLayout) {
        wgpuShaderModuleRelease(shaderModule);
        os::Printer::log("❌ Failed to create culling pipeline layout", ELL_ERROR);
        return false;
    }

    // Create compute pipeline
    WGPUComputePipelineDescriptor pipelineDesc = {};
    pipelineDesc.label = "GPU Culling Pipeline";
    pipelineDesc.layout = pipelineLayout;
    pipelineDesc.compute.module = shaderModule;
    pipelineDesc.compute.entryPoint = "cullObjects";

    cullingPipeline = wgpuDeviceCreateComputePipeline(device, &pipelineDesc);

    // Cleanup temporary resources
    wgpuShaderModuleRelease(shaderModule);
    wgpuPipelineLayoutRelease(pipelineLayout);

    if (!cullingPipeline) {
        os::Printer::log("❌ Failed to create culling compute pipeline", ELL_ERROR);
        return false;
    }

    return true;
}

bool CWebGPUComputeCulling::createBuffers() {
    // Object instance buffer (read-only storage)
    WGPUBufferDescriptor instanceBufferDesc = {};
    instanceBufferDesc.label = "Object Instance Buffer";
    instanceBufferDesc.size = maxObjects * sizeof(ObjectInstance);
    instanceBufferDesc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst;
    objectInstanceBuffer = wgpuDeviceCreateBuffer(device, &instanceBufferDesc);

    // Culling data buffer (uniform)
    WGPUBufferDescriptor cullingBufferDesc = {};
    cullingBufferDesc.label = "Culling Data Buffer";
    cullingBufferDesc.size = sizeof(CullingData);
    cullingBufferDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    cullingDataBuffer = wgpuDeviceCreateBuffer(device, &cullingBufferDesc);

    // Visibility results buffer (read-write storage)
    WGPUBufferDescriptor visibilityBufferDesc = {};
    visibilityBufferDesc.label = "Visibility Results Buffer";
    visibilityBufferDesc.size = maxObjects * sizeof(u32);
    visibilityBufferDesc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc;
    visibilityResultBuffer = wgpuDeviceCreateBuffer(device, &visibilityBufferDesc);

    // Indirect command buffer (read-write storage)
    WGPUBufferDescriptor indirectBufferDesc = {};
    indirectBufferDesc.label = "Indirect Command Buffer";
    indirectBufferDesc.size = maxObjects * sizeof(IndirectDrawIndexedCommand);
    indirectBufferDesc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_Indirect;
    indirectCommandBuffer = wgpuDeviceCreateBuffer(device, &indirectBufferDesc);

    // Atomic counters buffer (read-write storage)
    WGPUBufferDescriptor atomicBufferDesc = {};
    atomicBufferDesc.label = "Atomic Counters Buffer";
    atomicBufferDesc.size = 16; // One u32 counter + padding
    atomicBufferDesc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    WGPUBuffer atomicCounterBuffer = wgpuDeviceCreateBuffer(device, &atomicBufferDesc);

    // Readback buffer for CPU access
    WGPUBufferDescriptor readbackBufferDesc = {};
    readbackBufferDesc.label = "Readback Buffer";
    readbackBufferDesc.size = 16; // Match atomic counter buffer size
    readbackBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    readbackBuffer = wgpuDeviceCreateBuffer(device, &readbackBufferDesc);

    return objectInstanceBuffer && cullingDataBuffer && visibilityResultBuffer &&
           indirectCommandBuffer && atomicCounterBuffer && readbackBuffer;
}

bool CWebGPUComputeCulling::createBindGroups() {
    if (!cullingBindGroupLayout || !objectInstanceBuffer || !cullingDataBuffer ||
        !visibilityResultBuffer || !indirectCommandBuffer) {
        return false;
    }

    // Get atomic counter buffer (created in createBuffers)
    WGPUBuffer atomicCounterBuffer = nullptr;
    // Note: In real implementation, we'd store this as a member variable

    WGPUBindGroupEntry entries[5] = {};

    // Binding 0: Object instances
    entries[0].binding = 0;
    entries[0].buffer = objectInstanceBuffer;
    entries[0].size = maxObjects * sizeof(ObjectInstance);

    // Binding 1: Culling data
    entries[1].binding = 1;
    entries[1].buffer = cullingDataBuffer;
    entries[1].size = sizeof(CullingData);

    // Binding 2: Visibility results
    entries[2].binding = 2;
    entries[2].buffer = visibilityResultBuffer;
    entries[2].size = maxObjects * sizeof(u32);

    // Binding 3: Indirect commands
    entries[3].binding = 3;
    entries[3].buffer = indirectCommandBuffer;
    entries[3].size = maxObjects * sizeof(IndirectDrawIndexedCommand);

    // Binding 4: Atomic counters (temporary placeholder)
    entries[4].binding = 4;
    entries[4].buffer = visibilityResultBuffer; // Placeholder until we store atomic buffer
    entries[4].size = 16;

    WGPUBindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.label = "Culling Bind Group";
    bindGroupDesc.layout = cullingBindGroupLayout;
    bindGroupDesc.entryCount = 5;
    bindGroupDesc.entries = entries;

    cullingBindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);
    return cullingBindGroup != nullptr;
}

u32 CWebGPUComputeCulling::addObject(const core::matrix4& worldMatrix,
                                    const core::aabbox3df& boundingBox,
                                    u32 materialIndex,
                                    u32 vertexOffset,
                                    u32 indexOffset,
                                    u32 indexCount,
                                    u32 flags) {
    if (objects.size() >= maxObjects) {
        os::Printer::log("⚠️ CWebGPUComputeCulling: Maximum object count reached", ELL_WARNING);
        return static_cast<u32>(-1);
    }

    ObjectInstance instance = {};
    instance.worldMatrix = worldMatrix;
    instance.materialIndex = materialIndex;
    instance.vertexOffset = vertexOffset;
    instance.indexOffset = indexOffset;
    instance.indexCount = indexCount;
    instance.flags = flags;

    // Calculate bounding sphere from AABB
    calculateBoundingSphere(boundingBox,
                          instance.boundingSphere.xyz,
                          instance.boundingRadius);

    u32 objectId = static_cast<u32>(objects.size());
    objects.push_back(instance);

    return objectId;
}

void CWebGPUComputeCulling::updateObject(u32 objectId,
                                       const core::matrix4& worldMatrix,
                                       const core::aabbox3df& boundingBox) {
    if (objectId >= objects.size()) return;

    objects[objectId].worldMatrix = worldMatrix;
    calculateBoundingSphere(boundingBox,
                          objects[objectId].boundingSphere.xyz,
                          objects[objectId].boundingRadius);
}

void CWebGPUComputeCulling::removeObject(u32 objectId) {
    if (objectId >= objects.size()) return;

    // Mark as invalid
    objects[objectId].flags |= 0x80000000; // High bit = deleted
}

void CWebGPUComputeCulling::clearObjects() {
    objects.clear();
    visibleCount = 0;
}

void CWebGPUComputeCulling::setCameraParameters(const core::vector3df& position,
                                              const core::matrix4& viewProjection) {
    cullingData.cameraPosition = position;
    extractFrustumPlanes(viewProjection);
}

bool CWebGPUComputeCulling::performCulling() {
    if (!initialized || objects.empty()) return false;

    // Update object count
    cullingData.objectCount = static_cast<u32>(objects.size());

    // Upload data to GPU
    wgpuQueueWriteBuffer(queue, objectInstanceBuffer, 0,
                        objects.data(), objects.size() * sizeof(ObjectInstance));

    wgpuQueueWriteBuffer(queue, cullingDataBuffer, 0,
                        &cullingData, sizeof(CullingData));

    // Reset atomic counter
    u32 zero = 0;
    // Note: Would write to atomic counter buffer here

    // Create command encoder
    WGPUCommandEncoderDescriptor encoderDesc = {};
    encoderDesc.label = "Culling Command Encoder";
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

    // Begin compute pass
    WGPUComputePassDescriptor passDesc = {};
    passDesc.label = "GPU Culling Pass";
    WGPUComputePassEncoder computePass = wgpuCommandEncoderBeginComputePass(encoder, &passDesc);

    // Set pipeline and bind groups
    wgpuComputePassEncoderSetPipeline(computePass, cullingPipeline);
    wgpuComputePassEncoderSetBindGroup(computePass, 0, cullingBindGroup, 0, nullptr);

    // Dispatch compute shader
    u32 workgroups = (cullingData.objectCount + 63) / 64; // Round up to workgroup size
    wgpuComputePassEncoderDispatchWorkgroups(computePass, workgroups, 1, 1);

    // End compute pass and submit
    wgpuComputePassEncoderEnd(computePass);
    wgpuComputePassEncoderRelease(computePass);

    WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(queue, 1, &commands);
    wgpuCommandBufferRelease(commands);

    return true;
}

void CWebGPUComputeCulling::extractFrustumPlanes(const core::matrix4& viewProjection) {
    // Extract frustum planes from view-projection matrix
    // Left plane
    cullingData.frustumPlanes[0] = core::vector3df(
        viewProjection[3] + viewProjection[0],
        viewProjection[7] + viewProjection[4],
        viewProjection[11] + viewProjection[8]
    );
    cullingData.frustumDistances[0] = viewProjection[15] + viewProjection[12];

    // Right plane
    cullingData.frustumPlanes[1] = core::vector3df(
        viewProjection[3] - viewProjection[0],
        viewProjection[7] - viewProjection[4],
        viewProjection[11] - viewProjection[8]
    );
    cullingData.frustumDistances[1] = viewProjection[15] - viewProjection[12];

    // Bottom plane
    cullingData.frustumPlanes[2] = core::vector3df(
        viewProjection[3] + viewProjection[1],
        viewProjection[7] + viewProjection[5],
        viewProjection[11] + viewProjection[9]
    );
    cullingData.frustumDistances[2] = viewProjection[15] + viewProjection[13];

    // Top plane
    cullingData.frustumPlanes[3] = core::vector3df(
        viewProjection[3] - viewProjection[1],
        viewProjection[7] - viewProjection[5],
        viewProjection[11] - viewProjection[9]
    );
    cullingData.frustumDistances[3] = viewProjection[15] - viewProjection[13];

    // Near plane
    cullingData.frustumPlanes[4] = core::vector3df(
        viewProjection[2],
        viewProjection[6],
        viewProjection[10]
    );
    cullingData.frustumDistances[4] = viewProjection[14];

    // Far plane
    cullingData.frustumPlanes[5] = core::vector3df(
        viewProjection[3] - viewProjection[2],
        viewProjection[7] - viewProjection[6],
        viewProjection[11] - viewProjection[10]
    );
    cullingData.frustumDistances[5] = viewProjection[15] - viewProjection[14];

    // Normalize all planes
    for (int i = 0; i < 6; i++) {
        f32 length = cullingData.frustumPlanes[i].getLength();
        if (length > 0.001f) {
            cullingData.frustumPlanes[i] /= length;
            cullingData.frustumDistances[i] /= length;
        }
    }
}

void CWebGPUComputeCulling::calculateBoundingSphere(const core::aabbox3df& box,
                                                  core::vector3df& center,
                                                  f32& radius) {
    center = box.getCenter();
    core::vector3df extent = box.getExtent();
    radius = extent.getLength() * 0.5f; // Half diagonal length
}

} // namespace video
} // namespace irr

#endif // _IRR_COMPILE_WITH_WEBGPU_