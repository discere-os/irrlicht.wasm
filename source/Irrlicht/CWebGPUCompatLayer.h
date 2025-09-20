/*
 * WebGPU Compatibility Layer for Irrlicht Engine
 * Copyright (c) 2002-2012 Nikolaus Gebhardt
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license
 *
 * Ensures 100% API compatibility with existing Irrlicht applications
 * while leveraging WebGPU for modern GPU acceleration
 */

#ifndef IRR_C_WEBGPU_COMPAT_LAYER_H_INCLUDED
#define IRR_C_WEBGPU_COMPAT_LAYER_H_INCLUDED

#include "CWebGPUDriver.h"
#include <vector>
#include <queue>

#ifdef _IRR_COMPILE_WITH_WEBGPU_

namespace irr {
namespace video {

// Compatibility layer that makes WebGPU behave like OpenGL immediate mode
class CWebGPUCompatLayer : public CWebGPUDriver {
private:
    // Batch immediate mode calls for efficient WebGPU rendering
    struct RenderCall {
        const void* vertices;
        u32 vertexCount;
        const void* indexList;
        u32 primitiveCount;
        E_VERTEX_TYPE vType;
        scene::E_PRIMITIVE_TYPE pType;
        E_INDEX_TYPE iType;
        SMaterial material;
        core::matrix4 worldMatrix;
        u32 timestamp;
    };

    std::vector<RenderCall> batchedCalls;
    std::queue<WGPUBuffer> vertexBufferPool;
    std::queue<WGPUBuffer> indexBufferPool;

    // State tracking for OpenGL compatibility
    struct OpenGLCompatState {
        bool depthTestEnabled = true;
        bool depthWriteEnabled = true;
        bool blendEnabled = false;
        bool cullFaceEnabled = true;
        GLenum cullFaceMode = GL_BACK;
        GLenum frontFace = GL_CCW;
        GLenum blendSrc = GL_SRC_ALPHA;
        GLenum blendDst = GL_ONE_MINUS_SRC_ALPHA;
        bool lightingEnabled = true;
        SColor clearColor = SColor(255, 0, 0, 0);
        f32 clearDepth = 1.0f;
    } glState;

    // Fixed-function pipeline emulation
    struct FixedFunctionState {
        SLight lights[8];
        u32 activeLights = 0;
        SColor ambientLight = SColor(255, 64, 64, 64);
        bool fogEnabled = false;
        f32 fogStart = 0.0f;
        f32 fogEnd = 1.0f;
        SColor fogColor = SColor(255, 255, 255, 255);
    } ffState;

    // Command buffer management
    WGPUCommandEncoder currentEncoder = nullptr;
    WGPURenderPassEncoder currentRenderPass = nullptr;
    bool inScene = false;
    u32 optimalBatchSize = 100;

public:
    CWebGPUCompatLayer(const SIrrlichtCreationParameters& params,
                      io::IFileSystem* io,
                      CIrrDeviceSDL* device);

    virtual ~CWebGPUCompatLayer();

    // Perfect OpenGL API compatibility
    virtual bool beginScene(u16 clearFlag, SColor clearColor = SColor(255,0,0,0),
                           f32 clearDepth = 1.f, u8 clearStencil = 0,
                           const SExposedVideoData& videoData = SExposedVideoData(),
                           core::rect<s32>* sourceRect = 0) override;

    virtual bool endScene() override;

    // Immediate mode rendering with intelligent batching
    virtual void drawVertexPrimitiveList(const void* vertices, u32 vertexCount,
                                       const void* indexList, u32 primitiveCount,
                                       E_VERTEX_TYPE vType,
                                       scene::E_PRIMITIVE_TYPE pType,
                                       E_INDEX_TYPE iType) override;

    // Fixed-function transform compatibility
    virtual void setTransform(E_TRANSFORMATION_STATE state,
                            const core::matrix4& mat) override;

    // Material system compatibility
    virtual void setMaterial(const SMaterial& material) override;

    // Lighting system compatibility
    virtual void addDynamicLight(const SLight& light) override;
    virtual void turnLightOn(s32 lightIndex, bool turnOn) override;
    virtual void setAmbientLight(const SColorf& color) override;

    // Texture compatibility
    virtual void setTexture(u32 stage, ITexture* texture) override;
    virtual void setTextureCreationFlag(E_TEXTURE_CREATION_FLAG flag, bool enabled) override;

    // Render state compatibility
    virtual void enableClipPlane(u32 index, bool enable) override;
    virtual void setClipPlane(u32 index, const core::plane3df& plane,
                             bool enable = true) override;

    // Advanced compatibility features
    virtual void setRenderStatesStencilShadowMode(bool zfail, u32 debugDataVisible) override;
    virtual void setRenderStates3DMode() override;
    virtual void setRenderStates2DMode(bool alpha, bool texture, bool alphaChannel) override;

    // Fog compatibility
    virtual void setFog(SColor color = SColor(0,255,255,255),
                       E_FOG_TYPE fogType = EFT_FOG_LINEAR,
                       f32 start = 50.0f, f32 end = 100.0f, f32 density = 0.01f,
                       bool pixelFog = false, bool rangeFog = false) override;

private:
    // Batch management
    void flushRenderBatch();
    void optimizeBatch();
    bool shouldFlushBatch() const;

    // Resource management
    WGPUBuffer getPooledVertexBuffer(u32 size);
    WGPUBuffer getPooledIndexBuffer(u32 size);
    void returnBufferToPool(WGPUBuffer buffer, bool isVertexBuffer);

    // Fixed-function pipeline emulation
    WGPUShaderModule createFixedFunctionVertexShader(const SMaterial& material,
                                                    E_VERTEX_TYPE vType);
    WGPUShaderModule createFixedFunctionFragmentShader(const SMaterial& material);

    // State conversion
    WGPUBlendState convertBlendState(const SMaterial& material);
    WGPUDepthStencilState convertDepthStencilState(const SMaterial& material);
    WGPURasterizationState convertRasterizationState(const SMaterial& material);

    // OpenGL state emulation
    void updateGLState();
    void applyGLState();

    // Shader generation
    std::string generateVertexShaderGLSL(const SMaterial& material, E_VERTEX_TYPE vType);
    std::string generateFragmentShaderGLSL(const SMaterial& material);
    std::string convertGLSLToWGSL(const std::string& glslCode);
};

// Factory function that replaces OpenGL driver
IVideoDriver* createOpenGLDriver(const SIrrlichtCreationParameters& params,
                               io::IFileSystem* io, CIrrDeviceSDL* device);

// Compatibility aliases for seamless migration
#define COpenGLDriver CWebGPUCompatLayer
#define createWebGLDriver createWebGPUDriver

} // namespace video
} // namespace irr

#endif // _IRR_COMPILE_WITH_WEBGPU_

#endif // IRR_C_WEBGPU_COMPAT_LAYER_H_INCLUDED