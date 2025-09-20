/*
 * WebGPU Pipeline Management for Irrlicht Engine
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license
 */

#include "CWebGPUDriver.h"
#include <cstring>
#include <functional>

#ifdef _IRR_COMPILE_WITH_WEBGPU_

namespace irr {
namespace video {

// Embedded WGSL shader sources
const char* IRRLICHT_VERTEX_SHADER = R"(
struct IrrlichtTransforms {
  worldMatrix: mat4x4f,
  viewMatrix: mat4x4f,
  projectionMatrix: mat4x4f,
  normalMatrix: mat3x3f,
}

struct VertexInput {
  @location(0) position: vec3f,
  @location(1) normal: vec3f,
  @location(2) texCoord: vec2f,
  @location(3) color: vec4f,
}

struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) worldPos: vec3f,
  @location(1) normal: vec3f,
  @location(2) texCoord: vec2f,
  @location(3) color: vec4f,
}

@group(0) @binding(0) var<uniform> transforms: IrrlichtTransforms;

@vertex fn main(input: VertexInput) -> VertexOutput {
  let worldPos = transforms.worldMatrix * vec4f(input.position, 1.0);
  let viewPos = transforms.viewMatrix * worldPos;
  let clipPos = transforms.projectionMatrix * viewPos;

  var output: VertexOutput;
  output.position = clipPos;
  output.worldPos = worldPos.xyz;
  output.normal = normalize(transforms.normalMatrix * input.normal);
  output.texCoord = input.texCoord;
  output.color = input.color;
  return output;
}
)";

const char* IRRLICHT_FRAGMENT_SHADER = R"(
struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) worldPos: vec3f,
  @location(1) normal: vec3f,
  @location(2) texCoord: vec2f,
  @location(3) color: vec4f,
}

struct IrrlichtMaterial {
  diffuseColor: vec4f,
  specularColor: vec4f,
  emissiveColor: vec4f,
  shininess: f32,
  materialType: u32,
  lightingEnabled: u32,
  _padding: f32,
}

@group(0) @binding(1) var<uniform> material: IrrlichtMaterial;
@group(0) @binding(2) var diffuseTexture: texture_2d<f32>;
@group(0) @binding(3) var textureSampler: sampler;

@fragment fn main(input: VertexOutput) -> @location(0) vec4f {
  let textureColor = textureSample(diffuseTexture, textureSampler, input.texCoord);
  let baseColor = textureColor * input.color * material.diffuseColor;

  if (material.lightingEnabled == 0u) {
    return baseColor;
  }

  // Simple lighting calculation
  let lightDir = normalize(vec3f(1.0, 1.0, 1.0));
  let ndotl = max(dot(normalize(input.normal), lightDir), 0.0);
  let finalColor = baseColor.rgb * ndotl + material.emissiveColor.rgb;

  return vec4f(finalColor, baseColor.a);
}
)";

// Pipeline management methods for CWebGPUDriver
WGPURenderPipeline CWebGPUDriver::getOrCreatePipeline(const SMaterial& material,
                                                     E_VERTEX_TYPE vType,
                                                     scene::E_PRIMITIVE_TYPE pType) {
    uint64_t hash = calculatePipelineHash(material, vType, pType);

    auto it = gpu.pipelineCache.find(hash);
    if (it != gpu.pipelineCache.end()) {
        return it->second;
    }

    // Create new pipeline
    WGPURenderPipeline pipeline = createPipeline(material, vType, pType);
    if (pipeline) {
        gpu.pipelineCache[hash] = pipeline;
    }

    return pipeline;
}

WGPURenderPipeline CWebGPUDriver::createPipeline(const SMaterial& material,
                                                 E_VERTEX_TYPE vType,
                                                 scene::E_PRIMITIVE_TYPE pType) {

    // Create shader modules
    WGPUShaderModule vertexShader = getOrCreateShaderModule(IRRLICHT_VERTEX_SHADER, "Irrlicht Vertex Shader");
    WGPUShaderModule fragmentShader = getOrCreateShaderModule(IRRLICHT_FRAGMENT_SHADER, "Irrlicht Fragment Shader");

    if (!vertexShader || !fragmentShader) {
        return nullptr;
    }

    // Create bind group layout
    WGPUBindGroupLayoutEntry layoutEntries[4] = {};

    // Transforms uniform buffer
    layoutEntries[0].binding = 0;
    layoutEntries[0].visibility = WGPUShaderStage_Vertex;
    layoutEntries[0].buffer.type = WGPUBufferBindingType_Uniform;

    // Material uniform buffer
    layoutEntries[1].binding = 1;
    layoutEntries[1].visibility = WGPUShaderStage_Fragment;
    layoutEntries[1].buffer.type = WGPUBufferBindingType_Uniform;

    // Diffuse texture
    layoutEntries[2].binding = 2;
    layoutEntries[2].visibility = WGPUShaderStage_Fragment;
    layoutEntries[2].texture.sampleType = WGPUTextureSampleType_Float;
    layoutEntries[2].texture.viewDimension = WGPUTextureViewDimension_2D;

    // Texture sampler
    layoutEntries[3].binding = 3;
    layoutEntries[3].visibility = WGPUShaderStage_Fragment;
    layoutEntries[3].sampler.type = WGPUSamplerBindingType_Filtering;

    WGPUBindGroupLayoutDescriptor layoutDesc = {};
    layoutDesc.entryCount = 4;
    layoutDesc.entries = layoutEntries;
    layoutDesc.label = { "Irrlicht Bind Group Layout", WGPU_STRLEN };

    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(gpu.device, &layoutDesc);

    // Create pipeline layout
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &bindGroupLayout;
    pipelineLayoutDesc.label = { "Irrlicht Pipeline Layout", WGPU_STRLEN };

    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(gpu.device, &pipelineLayoutDesc);

    // Configure vertex attributes based on vertex type
    WGPUVertexAttribute attributes[4] = {};
    WGPUVertexBufferLayout vertexBufferLayout = {};

    switch (vType) {
        case EVT_STANDARD: // S3DVertex
            attributes[0] = { .format = WGPUVertexFormat_Float32x3, .offset = 0, .shaderLocation = 0 }; // position
            attributes[1] = { .format = WGPUVertexFormat_Float32x3, .offset = 12, .shaderLocation = 1 }; // normal
            attributes[2] = { .format = WGPUVertexFormat_Float32x2, .offset = 24, .shaderLocation = 2 }; // texCoord
            attributes[3] = { .format = WGPUVertexFormat_Unorm8x4, .offset = 32, .shaderLocation = 3 }; // color

            vertexBufferLayout.arrayStride = sizeof(S3DVertex);
            vertexBufferLayout.attributeCount = 4;
            vertexBufferLayout.attributes = attributes;
            break;

        case EVT_2TCOORDS: // S3DVertex2TCoords
            attributes[0] = { .format = WGPUVertexFormat_Float32x3, .offset = 0, .shaderLocation = 0 }; // position
            attributes[1] = { .format = WGPUVertexFormat_Float32x3, .offset = 12, .shaderLocation = 1 }; // normal
            attributes[2] = { .format = WGPUVertexFormat_Float32x2, .offset = 24, .shaderLocation = 2 }; // texCoord1
            attributes[3] = { .format = WGPUVertexFormat_Unorm8x4, .offset = 40, .shaderLocation = 3 }; // color

            vertexBufferLayout.arrayStride = sizeof(S3DVertex2TCoords);
            vertexBufferLayout.attributeCount = 4;
            vertexBufferLayout.attributes = attributes;
            break;

        case EVT_TANGENTS: // S3DVertexTangents
            attributes[0] = { .format = WGPUVertexFormat_Float32x3, .offset = 0, .shaderLocation = 0 }; // position
            attributes[1] = { .format = WGPUVertexFormat_Float32x3, .offset = 12, .shaderLocation = 1 }; // normal
            attributes[2] = { .format = WGPUVertexFormat_Float32x2, .offset = 24, .shaderLocation = 2 }; // texCoord
            attributes[3] = { .format = WGPUVertexFormat_Unorm8x4, .offset = 48, .shaderLocation = 3 }; // color

            vertexBufferLayout.arrayStride = sizeof(S3DVertexTangents);
            vertexBufferLayout.attributeCount = 4;
            vertexBufferLayout.attributes = attributes;
            break;
    }

    // Configure render pipeline
    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.label = { "Irrlicht Render Pipeline", WGPU_STRLEN };
    pipelineDesc.layout = pipelineLayout;

    // Vertex stage
    pipelineDesc.vertex.module = vertexShader;
    pipelineDesc.vertex.entryPoint = { "main", WGPU_STRLEN };
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;

    // Fragment stage
    WGPUFragmentState fragmentState = {};
    fragmentState.module = fragmentShader;
    fragmentState.entryPoint = { "main", WGPU_STRLEN };

    WGPUColorTargetState colorTarget = {};
    colorTarget.format = gpu.swapChainFormat;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    // Configure blending based on material
    if (material.MaterialType == EMT_TRANSPARENT_ALPHA_CHANNEL ||
        material.MaterialType == EMT_TRANSPARENT_VERTEX_ALPHA) {

        WGPUBlendState blendState = {};
        blendState.color.operation = WGPUBlendOperation_Add;
        blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
        blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
        blendState.alpha.operation = WGPUBlendOperation_Add;
        blendState.alpha.srcFactor = WGPUBlendFactor_One;
        blendState.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;

        colorTarget.blend = &blendState;
    }

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    pipelineDesc.fragment = &fragmentState;

    // Primitive state
    pipelineDesc.primitive.topology = getWebGPUTopology(pType);
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = material.BackfaceCulling ? WGPUCullMode_Back : WGPUCullMode_None;

    // Depth stencil state
    WGPUDepthStencilState depthStencilState = {};
    if (gpu.depthTexture) {
        depthStencilState.format = WGPUTextureFormat_Depth24Plus;
        depthStencilState.depthWriteEnabled = material.ZWriteEnable;
        depthStencilState.depthCompare = WGPUCompareFunction_Less;
        pipelineDesc.depthStencil = &depthStencilState;
    }

    // Multisample state
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = 0xFFFFFFFF;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(gpu.device, &pipelineDesc);

    // Cleanup
    wgpuBindGroupLayoutRelease(bindGroupLayout);
    wgpuPipelineLayoutRelease(pipelineLayout);

    return pipeline;
}

WGPUShaderModule CWebGPUDriver::getOrCreateShaderModule(const char* source, const char* label) {
    uint64_t hash = std::hash<std::string>{}(source);

    auto it = gpu.shaderCache.find(hash);
    if (it != gpu.shaderCache.end()) {
        return it->second;
    }

    WGPUShaderModuleDescriptor shaderDesc = {};
    shaderDesc.label = label;

    WGPUShaderModuleWGSLDescriptor wgslDesc = {};
    wgslDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgslDesc.code = source;
    shaderDesc.nextInChain = (WGPUChainedStruct*)&wgslDesc;

    WGPUShaderModule shader = wgpuDeviceCreateShaderModule(gpu.device, &shaderDesc);

    if (shader) {
        gpu.shaderCache[hash] = shader;
    }

    return shader;
}

WGPUBindGroup CWebGPUDriver::getOrCreateBindGroup(const SMaterial& material) {
    uint64_t hash = calculateBindGroupHash(material);

    auto it = gpu.bindGroupCache.find(hash);
    if (it != gpu.bindGroupCache.end()) {
        return it->second;
    }

    // Create bind group entries
    WGPUBindGroupEntry entries[4] = {};

    // Transform matrices
    entries[0].binding = 0;
    entries[0].buffer = gpu.transformBuffer;

    // Material properties
    entries[1].binding = 1;
    entries[1].buffer = gpu.materialBuffer;

    // Diffuse texture (use default if none specified)
    WGPUTexture defaultTexture = createDefaultTexture();
    WGPUTextureView textureView = wgpuTextureCreateView(defaultTexture, nullptr);
    entries[2].binding = 2;
    entries[2].textureView = textureView;

    // Sampler
    WGPUSamplerDescriptor samplerDesc = {};
    samplerDesc.addressModeU = WGPUAddressMode_Repeat;
    samplerDesc.addressModeV = WGPUAddressMode_Repeat;
    samplerDesc.magFilter = WGPUFilterMode_Linear;
    samplerDesc.minFilter = WGPUFilterMode_Linear;
    WGPUSampler sampler = wgpuDeviceCreateSampler(gpu.device, &samplerDesc);

    entries[3].binding = 3;
    entries[3].sampler = sampler;

    // Get bind group layout from first pipeline (assuming they're compatible)
    WGPUBindGroupLayout layout = nullptr; // Would get from pipeline

    WGPUBindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.layout = layout;
    bindGroupDesc.entryCount = 4;
    bindGroupDesc.entries = entries;
    bindGroupDesc.label = { "Irrlicht Material Bind Group", WGPU_STRLEN };

    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(gpu.device, &bindGroupDesc);

    if (bindGroup) {
        gpu.bindGroupCache[hash] = bindGroup;
    }

    return bindGroup;
}

uint64_t CWebGPUDriver::calculatePipelineHash(const SMaterial& material,
                                             E_VERTEX_TYPE vType,
                                             scene::E_PRIMITIVE_TYPE pType) {
    // Simple hash combining material and vertex type
    uint64_t hash = 0;
    hash ^= std::hash<uint32_t>{}((uint32_t)material.MaterialType) << 1;
    hash ^= std::hash<uint32_t>{}((uint32_t)vType) << 2;
    hash ^= std::hash<uint32_t>{}((uint32_t)pType) << 3;
    hash ^= std::hash<bool>{}(material.Lighting) << 4;
    hash ^= std::hash<bool>{}(material.ZWriteEnable) << 5;
    hash ^= std::hash<bool>{}(material.BackfaceCulling) << 6;
    return hash;
}

uint64_t CWebGPUDriver::calculateBindGroupHash(const SMaterial& material) {
    uint64_t hash = 0;
    hash ^= std::hash<uint32_t>{}(material.DiffuseColor.color);
    hash ^= std::hash<uint32_t>{}(material.SpecularColor.color);
    hash ^= std::hash<uint32_t>{}(material.EmissiveColor.color);
    hash ^= std::hash<float>{}(material.Shininess);
    // Add texture hash if available
    return hash;
}

WGPUTextureFormat CWebGPUDriver::getWebGPUFormat(ECOLOR_FORMAT format) {
    switch (format) {
        case ECF_A8R8G8B8: return WGPUTextureFormat_BGRA8Unorm;
        case ECF_R8G8B8: return WGPUTextureFormat_RGB8Unorm; // Not directly supported
        case ECF_A1R5G5B5: return WGPUTextureFormat_RGB5A1Unorm; // Not directly supported
        case ECF_R5G6B5: return WGPUTextureFormat_RGB565Unorm; // Not directly supported
        default: return WGPUTextureFormat_BGRA8Unorm;
    }
}

WGPUPrimitiveTopology CWebGPUDriver::getWebGPUTopology(scene::E_PRIMITIVE_TYPE pType) {
    switch (pType) {
        case scene::EPT_POINTS: return WGPUPrimitiveTopology_PointList;
        case scene::EPT_LINE_STRIP: return WGPUPrimitiveTopology_LineStrip;
        case scene::EPT_LINES: return WGPUPrimitiveTopology_LineList;
        case scene::EPT_TRIANGLE_STRIP: return WGPUPrimitiveTopology_TriangleStrip;
        case scene::EPT_TRIANGLES: return WGPUPrimitiveTopology_TriangleList;
        default: return WGPUPrimitiveTopology_TriangleList;
    }
}

WGPUTexture CWebGPUDriver::createDefaultTexture() {
    // Create 1x1 white texture as default
    WGPUTextureDescriptor textureDesc = {};
    textureDesc.size.width = 1;
    textureDesc.size.height = 1;
    textureDesc.size.depthOrArrayLayers = 1;
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = 1;
    textureDesc.dimension = WGPUTextureDimension_2D;
    textureDesc.format = WGPUTextureFormat_BGRA8Unorm;
    textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    textureDesc.label = { "Default White Texture", WGPU_STRLEN };

    WGPUTexture texture = wgpuDeviceCreateTexture(gpu.device, &textureDesc);

    // Upload white pixel data
    uint32_t whitePixel = 0xFFFFFFFF;
    WGPUImageCopyTexture dst = {};
    dst.texture = texture;

    WGPUTextureDataLayout layout = {};
    layout.bytesPerRow = 4;
    layout.rowsPerImage = 1;

    WGPUExtent3D size = {1, 1, 1};

    wgpuQueueWriteTexture(gpu.queue, &dst, &whitePixel, sizeof(whitePixel), &layout, &size);

    return texture;
}

void CWebGPUDriver::createSwapChain() {
    if (!gpu.device) return;

    // Get canvas from SDL or create default surface
    WGPUSurfaceDescriptor surfaceDesc = {};
    // Surface creation would be implemented based on platform

    WGPUSwapChainDescriptor swapChainDesc = {};
    swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
    swapChainDesc.format = WGPUTextureFormat_BGRA8Unorm;
    swapChainDesc.width = screenSize.Width;
    swapChainDesc.height = screenSize.Height;
    swapChainDesc.presentMode = WGPUPresentMode_Fifo;

    // Note: This is simplified - real implementation would handle surface creation
    gpu.swapChain = nullptr; // Would create actual swap chain
    gpu.swapChainFormat = WGPUTextureFormat_BGRA8Unorm;
}

void CWebGPUDriver::createDepthTexture() {
    if (!gpu.device) return;

    WGPUTextureDescriptor depthDesc = {};
    depthDesc.size.width = screenSize.Width;
    depthDesc.size.height = screenSize.Height;
    depthDesc.size.depthOrArrayLayers = 1;
    depthDesc.mipLevelCount = 1;
    depthDesc.sampleCount = 1;
    depthDesc.dimension = WGPUTextureDimension_2D;
    depthDesc.format = WGPUTextureFormat_Depth24Plus;
    depthDesc.usage = WGPUTextureUsage_RenderAttachment;
    depthDesc.label = { "Irrlicht Depth Buffer", WGPU_STRLEN };

    gpu.depthTexture = wgpuDeviceCreateTexture(gpu.device, &depthDesc);
    gpu.depthTextureView = wgpuTextureCreateView(gpu.depthTexture, nullptr);
}

} // namespace video
} // namespace irr

#endif // _IRR_COMPILE_WITH_WEBGPU_