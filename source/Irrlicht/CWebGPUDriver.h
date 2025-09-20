/*
 * WebGPU Driver Header for Irrlicht Engine
 * Copyright (c) 2002-2012 Nikolaus Gebhardt
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license (same as original Irrlicht Engine)
 */

#ifndef IRR_C_WEBGPU_DRIVER_H_INCLUDED
#define IRR_C_WEBGPU_DRIVER_H_INCLUDED

#include "IrrCompileConfig.h"

#ifdef _IRR_COMPILE_WITH_WEBGPU_

#include "CNullDriver.h"
#include "IMaterialRendererServices.h"
#include "SIrrCreationParameters.h"
#include "CWASMDependencyManager.h"
#include "SColor.h"
#include "SMaterial.h"
#include "S3DVertex.h"
#include "matrix4.h"
#include "dimension2d.h"

#include <webgpu/webgpu.h>
#include <unordered_map>

namespace irr {

class CIrrDeviceSDL;

namespace io {
    class IFileSystem;
}

namespace video {

class CWebGPUDriver : public CNullDriver {
public:
    CWebGPUDriver(const SIrrlichtCreationParameters& params,
                  io::IFileSystem* io,
                  CIrrDeviceSDL* device);

    virtual ~CWebGPUDriver();

    // Core video driver interface
    virtual bool beginScene(u16 clearFlag, SColor clearColor = SColor(255,0,0,0),
                           f32 clearDepth = 1.f, u8 clearStencil = 0,
                           const SExposedVideoData& videoData = SExposedVideoData(),
                           core::rect<s32>* sourceRect = 0) IRR_OVERRIDE;

    virtual bool endScene() IRR_OVERRIDE;

    virtual bool queryFeature(E_VIDEO_DRIVER_FEATURE feature) const IRR_OVERRIDE;

    virtual void disableFeature(E_VIDEO_DRIVER_FEATURE feature, bool flag=true) IRR_OVERRIDE;

    virtual const io::IAttributes& getDriverAttributes() const IRR_OVERRIDE;

    virtual void setTransform(E_TRANSFORMATION_STATE state, const core::matrix4& mat) IRR_OVERRIDE;

    virtual bool setRenderTarget(video::ITexture* texture, u16 clearFlag, SColor clearColor,
                                f32 clearDepth, u8 clearStencil) IRR_OVERRIDE;

    virtual void setMaterial(const SMaterial& material) IRR_OVERRIDE;

    virtual void drawVertexPrimitiveList(const void* vertices, u32 vertexCount,
                                       const void* indexList, u32 primitiveCount,
                                       E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType,
                                       E_INDEX_TYPE iType) IRR_OVERRIDE;

    virtual void drawMeshBuffer(const scene::IMeshBuffer* mb) IRR_OVERRIDE;

    virtual void draw2DVertexPrimitiveList(const void* vertices, u32 vertexCount,
                                         const void* indexList, u32 primitiveCount,
                                         E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType,
                                         E_INDEX_TYPE iType) IRR_OVERRIDE;

    // Texture management
    virtual ITexture* createTexture(const core::dimension2d<u32>& size,
                                   const io::path& name, ECOLOR_FORMAT format = ECF_A8R8G8B8);

    virtual ITexture* createTexture(const io::path& name, IImage* image);

    virtual ITexture* createRenderTargetTexture(const core::dimension2d<u32>& size,
                                               const io::path& name = "rt");

    // Driver identification
    virtual E_DRIVER_TYPE getDriverType() const IRR_OVERRIDE { return EDT_WEBGPU; }

    virtual const wchar_t* getName() const IRR_OVERRIDE { return L"WebGPU 1.0 (Irrlicht)"; }

    // Lighting support
    virtual s32 addDynamicLight(const SLight& light) IRR_OVERRIDE;
    virtual u32 getMaximalDynamicLightAmount() const IRR_OVERRIDE { return 8; }
    virtual void turnLightOn(s32 lightIndex, bool turnOn) IRR_OVERRIDE;

    // WebGPU-specific methods
    bool initializeWebGPU();
    WGPUDevice getWebGPUDevice() const { return gpu.device; }
    bool isOrchestrated() const { return gpu.orchestrated; }

private:
    struct WebGPUState {
        WGPUDevice device = nullptr;
        WGPUQueue queue = nullptr;
        WGPUSurface surface = nullptr;
        WGPUTextureFormat swapChainFormat = WGPUTextureFormat_BGRA8Unorm;

        // Render targets
        WGPUTexture depthTexture = nullptr;
        WGPUTextureView depthTextureView = nullptr;

        // Uniform buffers
        WGPUBuffer transformBuffer = nullptr;
        WGPUBuffer materialBuffer = nullptr;
        WGPUBuffer lightBuffer = nullptr;

        // Pipeline cache
        std::unordered_map<uint64_t, WGPURenderPipeline> pipelineCache;
        std::unordered_map<uint64_t, WGPUBindGroup> bindGroupCache;
        std::unordered_map<uint64_t, WGPUShaderModule> shaderCache;

        // Current state
        core::matrix4 matrices[ETS_COUNT];
        SMaterial currentMaterial;
        bool matrixChanged[ETS_COUNT];

        // External orchestrator integration
        void* externalContext = nullptr;
        void* computeContext = nullptr;
        bool orchestrated = false;

        // Render state
        bool inScene = false;
        SColor clearColor = SColor(255,0,0,0);
        f32 clearDepth = 1.0f;
        u8 clearStencil = 0;
    } gpu;

    core::dimension2d<u32> screenSize;

    // Initialization methods
    bool initializeStandalone();
    bool initializeWithOrchestrator();

    // Resource creation
    void createSwapChain();
    void createDepthTexture();
    void createUniformBuffers();

    // Pipeline management
    WGPURenderPipeline getOrCreatePipeline(const SMaterial& material, E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType);
    WGPURenderPipeline createPipeline(const SMaterial& material, E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType);
    WGPUBindGroup getOrCreateBindGroup(const SMaterial& material);
    WGPUShaderModule getOrCreateShaderModule(const char* source, const char* label);

    // State updates
    void updateTransformUniforms();
    void updateMaterialUniforms();
    void updateLightUniforms();

    // Hash calculation for caching
    uint64_t calculatePipelineHash(const SMaterial& material, E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType);
    uint64_t calculateBindGroupHash(const SMaterial& material);

    // Buffer creation
    WGPUBuffer createVertexBuffer(const void* vertices, u32 vertexCount, E_VERTEX_TYPE vType);
    WGPUBuffer createIndexBuffer(const void* indices, u32 indexCount, E_INDEX_TYPE iType);

    // Shader source generation
    const char* getVertexShaderSource(const SMaterial& material, E_VERTEX_TYPE vType);
    const char* getFragmentShaderSource(const SMaterial& material);

    // Utility functions
    size_t getVertexTypeSize(E_VERTEX_TYPE vType);
    WGPUTextureFormat getWebGPUFormat(ECOLOR_FORMAT format);
    WGPUPrimitiveTopology getWebGPUTopology(scene::E_PRIMITIVE_TYPE pType);
    WGPUBlendState getWebGPUBlendState(const SMaterial& material);
};

// Factory function declaration
IVideoDriver* createWebGPUDriver(const SIrrlichtCreationParameters& params,
                                io::IFileSystem* io, CIrrDeviceSDL* device);

} // namespace video
} // namespace irr

#endif // _IRR_COMPILE_WITH_WEBGPU_

#endif // IRR_C_WEBGPU_DRIVER_H_INCLUDED