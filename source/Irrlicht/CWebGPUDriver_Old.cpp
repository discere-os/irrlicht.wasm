/*
 * WebGPU Driver Implementation for Irrlicht Engine
 * Copyright (c) 2002-2012 Nikolaus Gebhardt
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license (same as original Irrlicht Engine)
 *
 * WebGPU-native rendering backend with external orchestrator integration
 */

#include "CWebGPUDriver.h"
#include "CNullDriver.h"
#include "CWASMDependencyManager.h"
#include "SExposedVideoData.h"
#include "IFileSystem.h"
#include "os.h"
#include "CImage.h"
#include "CColorConverter.h"

#ifdef _IRR_COMPILE_WITH_WEBGPU_

#include <webgpu/webgpu.h>
#include <emscripten/bind.h>
#include <unordered_map>
#include <string>

namespace irr {
namespace video {

// WebGPU orchestrator integration using extern "C" functions
extern "C" {
    void* get_external_webgpu_context() {
        EM_ASM({
            if (typeof Module.externalWebGPUContext === 'undefined') {
                console.log('🎮 Irrlicht: No external WebGPU orchestrator found - using standalone WebGPU');
                return null;
            }
            console.log('🎮 Irrlicht: External WebGPU orchestrator detected');
            return Module.externalWebGPUContext;
        });
        return nullptr;
    }

    void* request_webgpu_device(const char* label, int priority) {
        return (void*)EM_ASM_INT({
            if (!Module.externalWebGPUContext?.requestDevice) {
                return 0;
            }

            const device = Module.externalWebGPUContext.requestDevice({
                label: UTF8ToString($0),
                priority: $1,
                features: ['bgra8unorm-storage', 'timestamp-query'],
                limits: {
                    maxTexture2DSize: 4096,
                    maxUniformBufferBindingSize: 64 * 1024,
                    maxStorageBufferBindingSize: 16 * 1024 * 1024
                }
            });

            return device || 0;
        }, label, priority);
    }

    void* request_compute_context(const char* shader_key, int memory_size) {
        return (void*)EM_ASM_INT({
            if (!Module.externalWebGPUContext?.requestComputeContext) {
                return 0;
            }

            return Module.externalWebGPUContext.requestComputeContext({
                shaderKey: UTF8ToString($0),
                priority: 2, // High priority for 3D rendering
                memorySize: $1
            });
        }, shader_key, memory_size);
    }
}

// WebGPU Driver Implementation
CWebGPUDriver::CWebGPUDriver(const SIrrlichtCreationParameters& params,
                             io::IFileSystem* io,
                             CIrrDeviceSDL* device)
    : CNullDriver(io, params.WindowSize), screenSize(params.WindowSize) {

    memset(&gpu, 0, sizeof(gpu));
private:
    struct WebGPUState {
        WGPUDevice device = nullptr;
        WGPUQueue queue = nullptr;
        WGPUSwapChain swapChain = nullptr;
        WGPUTextureFormat swapChainFormat = WGPUTextureFormat_BGRA8Unorm;

        // Render targets
        WGPUTexture depthTexture = nullptr;
        WGPUTextureView depthTextureView = nullptr;

        // Uniform buffers
        WGPUBuffer transformBuffer = nullptr;  // World, View, Projection matrices
        WGPUBuffer materialBuffer = nullptr;   // Material properties
        WGPUBuffer lightBuffer = nullptr;     // Lighting data

        // Pipeline cache for performance
        std::unordered_map<uint64_t, WGPURenderPipeline> pipelineCache;
        std::unordered_map<uint64_t, WGPUBindGroup> bindGroupCache;

        // Current state
        core::matrix4 matrices[ETS_COUNT];
        SMaterial currentMaterial;
        bool matrixChanged[ETS_COUNT];

        // External orchestrator integration
        void* externalContext = nullptr;
        void* computeContext = nullptr;
        bool orchestrated = false;
    } gpu;

    core::dimension2d<u32> screenSize;

public:
    CWebGPUDriver(const SIrrlichtCreationParameters& params,
                  io::IFileSystem* io,
                  CIrrDeviceSDL* device);

    virtual ~CWebGPUDriver();

    // Core video driver interface
    virtual bool beginScene(u16 clearFlag, SColor clearColor = SColor(255,0,0,0),
                           f32 clearDepth = 1.f, u8 clearStencil = 0,
                           const SExposedVideoData& videoData = SExposedVideoData(),
                           core::rect<s32>* sourceRect = 0) override;

    virtual bool endScene() override;

    virtual void setTransform(E_TRANSFORMATION_STATE state, const core::matrix4& mat) override;

    virtual void setMaterial(const SMaterial& material) override;

    virtual void drawVertexPrimitiveList(const void* vertices, u32 vertexCount,
                                       const void* indexList, u32 primitiveCount,
                                       E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType,
                                       E_INDEX_TYPE iType) override;

    // Texture management
    virtual ITexture* createTexture(const core::dimension2d<u32>& size,
                                   const io::path& name, ECOLOR_FORMAT format = ECF_A8R8G8B8) override;

    virtual ITexture* createTexture(const io::path& name, IImage* image) override;

    // Feature queries
    virtual bool queryFeature(E_VIDEO_DRIVER_FEATURE feature) const override;

    virtual E_DRIVER_TYPE getDriverType() const override { return EDT_WEBGPU; }

    virtual const wchar_t* getName() const override { return L"WebGPU 1.0"; }

private:
    bool initializeWebGPU();
    bool initializeStandalone();
    bool initializeWithOrchestrator();

    void createSwapChain();
    void createDepthTexture();
    void createUniformBuffers();

    WGPURenderPipeline getOrCreatePipeline(const SMaterial& material, E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType);
    WGPUBindGroup getOrCreateBindGroup(const SMaterial& material);

    void updateTransformUniforms();
    void updateMaterialUniforms();

    uint64_t calculatePipelineHash(const SMaterial& material, E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType);
    uint64_t calculateBindGroupHash(const SMaterial& material);

    WGPUBuffer createVertexBuffer(const void* vertices, u32 vertexCount, E_VERTEX_TYPE vType);
    WGPUBuffer createIndexBuffer(const void* indices, u32 indexCount, E_INDEX_TYPE iType);

    const char* getVertexShaderSource(const SMaterial& material, E_VERTEX_TYPE vType);
    const char* getFragmentShaderSource(const SMaterial& material);
};

// Constructor
CWebGPUDriver::CWebGPUDriver(const SIrrlichtCreationParameters& params,
                            io::IFileSystem* io,
                            CIrrDeviceSDL* device)
    : CNullDriver(io, params.WindowSize), screenSize(params.WindowSize)
{
    #ifdef _DEBUG
    setDebugName("CWebGPUDriver");
    #endif

    // Initialize matrix tracking
    for (int i = 0; i < ETS_COUNT; ++i) {
        gpu.matrices[i].makeIdentity();
        gpu.matrixChanged[i] = true;
    }

    // Initialize WebGPU
    if (!initializeWebGPU()) {
        os::Printer::log("Failed to initialize WebGPU", ELL_ERROR);
    }
}

// Destructor
CWebGPUDriver::~CWebGPUDriver() {
    // Clean up WebGPU resources
    if (gpu.depthTextureView) wgpuTextureViewRelease(gpu.depthTextureView);
    if (gpu.depthTexture) wgpuTextureRelease(gpu.depthTexture);
    if (gpu.transformBuffer) wgpuBufferRelease(gpu.transformBuffer);
    if (gpu.materialBuffer) wgpuBufferRelease(gpu.materialBuffer);
    if (gpu.lightBuffer) wgpuBufferRelease(gpu.lightBuffer);

    for (auto& [hash, pipeline] : gpu.pipelineCache) {
        wgpuRenderPipelineRelease(pipeline);
    }

    for (auto& [hash, bindGroup] : gpu.bindGroupCache) {
        wgpuBindGroupRelease(bindGroup);
    }

    if (gpu.swapChain) wgpuSwapChainRelease(gpu.swapChain);
    if (gpu.queue) wgpuQueueRelease(gpu.queue);
    if (gpu.device) wgpuDeviceRelease(gpu.device);
}

// WebGPU initialization
bool CWebGPUDriver::initializeWebGPU() {
    // Try external orchestrator first
    gpu.externalContext = get_external_webgpu_context();

    if (gpu.externalContext) {
        gpu.orchestrated = true;
        return initializeWithOrchestrator();
    } else {
        gpu.orchestrated = false;
        return initializeStandalone();
    }
}

bool CWebGPUDriver::initializeWithOrchestrator() {
    os::Printer::log("🎮 Initializing WebGPU with external orchestrator", ELL_INFORMATION);

    // Request device from orchestrator
    gpu.device = (WGPUDevice)request_webgpu_device("Irrlicht 3D Engine", 2);
    if (!gpu.device) {
        os::Printer::log("Failed to get WebGPU device from orchestrator", ELL_ERROR);
        return false;
    }

    // Request compute context for advanced rendering
    gpu.computeContext = request_compute_context("irrlicht_3d_rendering", 8 * 1024 * 1024);
    if (!gpu.computeContext) {
        os::Printer::log("Warning: No compute context available", ELL_WARNING);
    }

    gpu.queue = wgpuDeviceGetQueue(gpu.device);

    createSwapChain();
    createDepthTexture();
    createUniformBuffers();

    os::Printer::log("✓ WebGPU initialized successfully with orchestrator", ELL_INFORMATION);
    return true;
}

bool CWebGPUDriver::initializeStandalone() {
    os::Printer::log("🎮 Initializing standalone WebGPU", ELL_INFORMATION);

    // Standard WebGPU initialization
    WGPUInstanceDescriptor instanceDesc = {};
    WGPUInstance instance = wgpuCreateInstance(&instanceDesc);

    WGPURequestAdapterOptions adapterOptions = {};
    adapterOptions.powerPreference = WGPUPowerPreference_HighPerformance;

    // Note: This would need async handling in real implementation
    // For now, assume synchronous initialization
    WGPUAdapter adapter = nullptr; // Would be obtained async

    if (!adapter) {
        os::Printer::log("No WebGPU adapter available", ELL_ERROR);
        return false;
    }

    WGPUDeviceDescriptor deviceDesc = {};
    const char* featureNames[] = {"bgra8unorm-storage"};
    deviceDesc.requiredFeatureCount = 1;
    deviceDesc.requiredFeatures = (WGPUFeatureName*)featureNames;

    gpu.device = nullptr; // Would be obtained async
    if (!gpu.device) {
        os::Printer::log("Failed to create WebGPU device", ELL_ERROR);
        return false;
    }

    gpu.queue = wgpuDeviceGetQueue(gpu.device);

    createSwapChain();
    createDepthTexture();
    createUniformBuffers();

    os::Printer::log("✓ Standalone WebGPU initialized", ELL_INFORMATION);
    return true;
}

void CWebGPUDriver::createUniformBuffers() {
    // Transform matrices buffer (World, View, Projection + Normal matrix)
    WGPUBufferDescriptor transformBufferDesc = {};
    transformBufferDesc.size = sizeof(float) * 16 * 4; // 4 matrices * 16 floats
    transformBufferDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    transformBufferDesc.label = "Irrlicht Transform Matrices";
    gpu.transformBuffer = wgpuDeviceCreateBuffer(gpu.device, &transformBufferDesc);

    // Material properties buffer
    WGPUBufferDescriptor materialBufferDesc = {};
    materialBufferDesc.size = 256; // SMaterial properties
    materialBufferDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    materialBufferDesc.label = "Irrlicht Material Properties";
    gpu.materialBuffer = wgpuDeviceCreateBuffer(gpu.device, &materialBufferDesc);

    // Lighting buffer
    WGPUBufferDescriptor lightBufferDesc = {};
    lightBufferDesc.size = 1024; // Multiple lights
    lightBufferDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    lightBufferDesc.label = "Irrlicht Lighting Data";
    gpu.lightBuffer = wgpuDeviceCreateBuffer(gpu.device, &lightBufferDesc);
}

bool CWebGPUDriver::beginScene(u16 clearFlag, SColor clearColor, f32 clearDepth, u8 clearStencil,
                              const SExposedVideoData& videoData, core::rect<s32>* sourceRect) {
    CNullDriver::beginScene(clearFlag, clearColor, clearDepth, clearStencil, videoData, sourceRect);

    // Update any changed matrices
    updateTransformUniforms();

    return true;
}

bool CWebGPUDriver::endScene() {
    CNullDriver::endScene();

    // Present the frame
    if (gpu.swapChain) {
        wgpuSwapChainPresent(gpu.swapChain);
    }

    return true;
}

void CWebGPUDriver::setTransform(E_TRANSFORMATION_STATE state, const core::matrix4& mat) {
    if (state >= ETS_COUNT) return;

    gpu.matrices[state] = mat;
    gpu.matrixChanged[state] = true;

    CNullDriver::setTransform(state, mat);
}

void CWebGPUDriver::updateTransformUniforms() {
    bool needsUpdate = false;
    for (int i = 0; i < ETS_COUNT; ++i) {
        if (gpu.matrixChanged[i]) {
            needsUpdate = true;
            gpu.matrixChanged[i] = false;
        }
    }

    if (!needsUpdate) return;

    // Pack matrices for GPU upload
    float matrixData[64]; // 4 matrices * 16 floats

    // World matrix
    gpu.matrices[ETS_WORLD].getFloats(matrixData);

    // View matrix
    gpu.matrices[ETS_VIEW].getFloats(matrixData + 16);

    // Projection matrix
    gpu.matrices[ETS_PROJECTION].getFloats(matrixData + 32);

    // Normal matrix (inverse transpose of world matrix upper 3x3)
    core::matrix4 normalMatrix = gpu.matrices[ETS_WORLD];
    normalMatrix.getInverse();
    normalMatrix = normalMatrix.getTransposed();
    normalMatrix.getFloats(matrixData + 48);

    // Upload to GPU
    wgpuQueueWriteBuffer(gpu.queue, gpu.transformBuffer, 0, matrixData, sizeof(matrixData));
}

void CWebGPUDriver::setMaterial(const SMaterial& material) {
    CNullDriver::setMaterial(material);

    gpu.currentMaterial = material;
    updateMaterialUniforms();
}

void CWebGPUDriver::updateMaterialUniforms() {
    struct MaterialData {
        float diffuseColor[4];
        float specularColor[4];
        float emissiveColor[4];
        float shininess;
        uint32_t materialType;
        uint32_t flags;
        float _padding[1];
    } materialData;

    // Convert Irrlicht material to WebGPU format
    materialData.diffuseColor[0] = gpu.currentMaterial.DiffuseColor.getRed() / 255.0f;
    materialData.diffuseColor[1] = gpu.currentMaterial.DiffuseColor.getGreen() / 255.0f;
    materialData.diffuseColor[2] = gpu.currentMaterial.DiffuseColor.getBlue() / 255.0f;
    materialData.diffuseColor[3] = gpu.currentMaterial.DiffuseColor.getAlpha() / 255.0f;

    materialData.specularColor[0] = gpu.currentMaterial.SpecularColor.getRed() / 255.0f;
    materialData.specularColor[1] = gpu.currentMaterial.SpecularColor.getGreen() / 255.0f;
    materialData.specularColor[2] = gpu.currentMaterial.SpecularColor.getBlue() / 255.0f;
    materialData.specularColor[3] = gpu.currentMaterial.SpecularColor.getAlpha() / 255.0f;

    materialData.emissiveColor[0] = gpu.currentMaterial.EmissiveColor.getRed() / 255.0f;
    materialData.emissiveColor[1] = gpu.currentMaterial.EmissiveColor.getGreen() / 255.0f;
    materialData.emissiveColor[2] = gpu.currentMaterial.EmissiveColor.getBlue() / 255.0f;
    materialData.emissiveColor[3] = gpu.currentMaterial.EmissiveColor.getAlpha() / 255.0f;

    materialData.shininess = gpu.currentMaterial.Shininess;
    materialData.materialType = (uint32_t)gpu.currentMaterial.MaterialType;
    materialData.flags = gpu.currentMaterial.Lighting ? 1 : 0;

    wgpuQueueWriteBuffer(gpu.queue, gpu.materialBuffer, 0, &materialData, sizeof(materialData));
}

void CWebGPUDriver::drawVertexPrimitiveList(const void* vertices, u32 vertexCount,
                                           const void* indexList, u32 primitiveCount,
                                           E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType,
                                           E_INDEX_TYPE iType) {

    if (!gpu.device || !vertices || vertexCount == 0) return;

    // Get or create pipeline for current state
    WGPURenderPipeline pipeline = getOrCreatePipeline(gpu.currentMaterial, vType, pType);
    if (!pipeline) return;

    // Create vertex and index buffers
    WGPUBuffer vertexBuffer = createVertexBuffer(vertices, vertexCount, vType);
    WGPUBuffer indexBuffer = nullptr;
    uint32_t indexCount = 0;

    if (indexList && primitiveCount > 0) {
        indexCount = primitiveCount * 3; // Assuming triangles
        indexBuffer = createIndexBuffer(indexList, indexCount, iType);
    }

    // Get current render target
    WGPUTexture backBuffer = wgpuSwapChainGetCurrentTexture(gpu.swapChain);
    WGPUTextureView backBufferView = wgpuTextureCreateView(backBuffer, nullptr);

    // Create render pass
    WGPURenderPassColorAttachment colorAttachment = {};
    colorAttachment.view = backBufferView;
    colorAttachment.loadOp = WGPULoadOp_Load; // Don't clear, preserve previous content
    colorAttachment.storeOp = WGPUStoreOp_Store;

    WGPURenderPassDepthStencilAttachment depthAttachment = {};
    if (gpu.depthTextureView) {
        depthAttachment.view = gpu.depthTextureView;
        depthAttachment.depthLoadOp = WGPULoadOp_Load;
        depthAttachment.depthStoreOp = WGPUStoreOp_Store;
    }

    WGPURenderPassDescriptor renderPassDesc = {};
    renderPassDesc.label = "Irrlicht Draw Call";
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &colorAttachment;
    if (gpu.depthTextureView) {
        renderPassDesc.depthStencilAttachment = &depthAttachment;
    }

    // Record commands
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(gpu.device, nullptr);
    WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);

    wgpuRenderPassEncoderSetPipeline(renderPass, pipeline);

    // Bind resources
    WGPUBindGroup bindGroup = getOrCreateBindGroup(gpu.currentMaterial);
    if (bindGroup) {
        wgpuRenderPassEncoderSetBindGroup(renderPass, 0, bindGroup, 0, nullptr);
    }

    // Set vertex buffer
    size_t vertexSize = getVertexTypeSize(vType);
    wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vertexBuffer, 0, vertexCount * vertexSize);

    // Draw
    if (indexBuffer && indexCount > 0) {
        wgpuRenderPassEncoderSetIndexBuffer(renderPass, indexBuffer,
            iType == EIT_32BIT ? WGPUIndexFormat_Uint32 : WGPUIndexFormat_Uint16, 0, indexCount * (iType == EIT_32BIT ? 4 : 2));
        wgpuRenderPassEncoderDrawIndexed(renderPass, indexCount, 1, 0, 0, 0);
    } else {
        wgpuRenderPassEncoderDraw(renderPass, vertexCount, 1, 0, 0);
    }

    wgpuRenderPassEncoderEnd(renderPass);

    WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(gpu.queue, 1, &commands);

    // Clean up
    wgpuTextureViewRelease(backBufferView);
    wgpuTextureRelease(backBuffer);
    wgpuBufferRelease(vertexBuffer);
    if (indexBuffer) wgpuBufferRelease(indexBuffer);
    wgpuCommandEncoderRelease(encoder);
    wgpuRenderPassEncoderRelease(renderPass);
    wgpuCommandBufferRelease(commands);
}

// Helper function to get vertex size
size_t getVertexTypeSize(E_VERTEX_TYPE vType) {
    switch (vType) {
        case EVT_STANDARD: return sizeof(S3DVertex);
        case EVT_2TCOORDS: return sizeof(S3DVertex2TCoords);
        case EVT_TANGENTS: return sizeof(S3DVertexTangents);
        default: return sizeof(S3DVertex);
    }
}

} // namespace video
} // namespace irr

// Factory function for WebGPU driver creation
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    irr::IrrlichtDevice* irrlicht_create_webgpu_device(int width, int height) {
        irr::SIrrlichtCreationParameters params;
        params.DriverType = irr::video::EDT_WEBGPU;
        params.WindowSize = irr::core::dimension2d<irr::u32>(width, height);
        params.Bits = 32;
        params.Fullscreen = false;
        params.Stencilbuffer = true;
        params.Vsync = false;

        return irr::createDeviceEx(params);
    }
}

#endif // _IRR_COMPILE_WITH_WEBGPU_