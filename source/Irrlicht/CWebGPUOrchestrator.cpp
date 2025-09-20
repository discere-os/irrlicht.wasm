/*
 * WebGPU Orchestrator Integration for Irrlicht.wasm
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license
 *
 * Handles integration with external WebGPU orchestrators for
 * GPU resource sharing and coordination across applications
 */

#include <emscripten.h>
#include <webgpu/webgpu.h>
#include <cstdio>

namespace irr {
namespace webgpu {

// JavaScript interface for external orchestrator communication
EM_JS(void, register_irrlicht_with_orchestrator, (), {
    if (typeof Module.externalWebGPUContext === 'undefined') {
        Module.externalWebGPUContext = {};
    }

    Module.externalWebGPUContext.irrlichtEngine = {
        name: 'Irrlicht 3D Engine',
        version: '1.9.0',
        capabilities: {
            supports3D: true,
            supportsCompute: true,
            requiresHighPerformance: true,
            memoryRequirement: 16 * 1024 * 1024, // 16MB baseline
            preferredGPUTier: 'high-end'
        },
        status: 'initializing'
    };

    console.log('🎮 Irrlicht registered with external WebGPU orchestrator');
});

EM_JS(int, check_orchestrator_capabilities, (), {
    if (!Module.externalWebGPUContext?.getCapabilities) {
        return 0; // No orchestrator
    }

    const caps = Module.externalWebGPUContext.getCapabilities();
    console.log('🔍 Orchestrator capabilities:', caps);

    // Check if orchestrator supports our requirements
    const hasWebGPU = caps.webgpu && caps.webgpu.supported;
    const hasHighPerformance = caps.gpu && caps.gpu.tier !== 'low-end';
    const hasComputeShaders = caps.features && caps.features.includes('compute-shaders');

    return hasWebGPU && hasHighPerformance ? 1 : 0;
});

EM_JS(void*, request_3d_rendering_context, (const char* app_name), {
    if (!Module.externalWebGPUContext?.request3DContext) {
        console.warn('🎮 No 3D context provider available in orchestrator');
        return null;
    }

    const context = Module.externalWebGPUContext.request3DContext({
        applicationName: UTF8ToString(app_name),
        contextType: '3d-engine',
        priority: 'high',
        requirements: {
            minTextureSize: 1024,
            maxTextureSize: 4096,
            supportsDepthTextures: true,
            supportsMipmaps: true,
            supportsMultisampling: true,
            preferredSampleCount: 4
        },
        memoryLimits: {
            uniformBuffers: 64 * 1024,      // 64KB
            storageBuffers: 16 * 1024 * 1024, // 16MB
            textures: 256 * 1024 * 1024      // 256MB
        }
    });

    if (context) {
        console.log('✓ 3D rendering context acquired from orchestrator');
        Module.externalWebGPUContext.irrlichtEngine.status = 'context-acquired';
    }

    return context || null;
});

EM_JS(void*, request_compute_pipeline, (const char* shader_name, int workgroup_size), {
    if (!Module.externalWebGPUContext?.requestComputePipeline) {
        return null;
    }

    return Module.externalWebGPUContext.requestComputePipeline({
        shaderName: UTF8ToString(shader_name),
        workgroupSize: workgroup_size,
        bufferTypes: ['uniform', 'storage'],
        priority: 'normal'
    });
});

EM_JS(void, notify_frame_start, (), {
    if (Module.externalWebGPUContext?.notifyFrameStart) {
        Module.externalWebGPUContext.notifyFrameStart('irrlicht-3d-engine');
    }
});

EM_JS(void, notify_frame_end, (), {
    if (Module.externalWebGPUContext?.notifyFrameEnd) {
        Module.externalWebGPUContext.notifyFrameEnd('irrlicht-3d-engine');
    }
});

EM_JS(void, request_gpu_memory, (int size_bytes), {
    if (Module.externalWebGPUContext?.requestGPUMemory) {
        Module.externalWebGPUContext.requestGPUMemory({
            requester: 'irrlicht-3d-engine',
            sizeBytes: size_bytes,
            usage: 'vertex-index-buffers'
        });
    }
});

EM_JS(void, release_gpu_memory, (int size_bytes), {
    if (Module.externalWebGPUContext?.releaseGPUMemory) {
        Module.externalWebGPUContext.releaseGPUMemory({
            requester: 'irrlicht-3d-engine',
            sizeBytes: size_bytes
        });
    }
});

class OrchestratorIntegration {
private:
    void* renderingContext = nullptr;
    void* computeContext = nullptr;
    bool initialized = false;

public:
    static OrchestratorIntegration& getInstance() {
        static OrchestratorIntegration instance;
        return instance;
    }

    bool initialize() {
        if (initialized) return true;

        // Register with orchestrator
        register_irrlicht_with_orchestrator();

        // Check if orchestrator supports our requirements
        if (!check_orchestrator_capabilities()) {
            printf("⚠️  Orchestrator doesn't meet Irrlicht's requirements - using standalone mode\n");
            return false;
        }

        // Request 3D rendering context
        renderingContext = request_3d_rendering_context("Irrlicht 3D Engine");
        if (!renderingContext) {
            printf("❌ Failed to acquire 3D rendering context from orchestrator\n");
            return false;
        }

        // Request compute context for advanced features
        computeContext = request_compute_pipeline("irrlicht-vertex-processing", 64);
        if (!computeContext) {
            printf("⚠️  No compute context available - advanced features disabled\n");
        }

        initialized = true;
        printf("✅ Irrlicht successfully integrated with WebGPU orchestrator\n");
        return true;
    }

    void beginFrame() {
        if (initialized) {
            notify_frame_start();
        }
    }

    void endFrame() {
        if (initialized) {
            notify_frame_end();
        }
    }

    void requestMemory(int sizeBytes) {
        if (initialized) {
            request_gpu_memory(sizeBytes);
        }
    }

    void releaseMemory(int sizeBytes) {
        if (initialized) {
            release_gpu_memory(sizeBytes);
        }
    }

    void* getRenderingContext() const { return renderingContext; }
    void* getComputeContext() const { return computeContext; }
    bool isInitialized() const { return initialized; }
};

} // namespace webgpu
} // namespace irr

// C API for external access
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    int irrlicht_init_orchestrator() {
        return irr::webgpu::OrchestratorIntegration::getInstance().initialize() ? 1 : 0;
    }

    EMSCRIPTEN_KEEPALIVE
    void irrlicht_orchestrator_begin_frame() {
        irr::webgpu::OrchestratorIntegration::getInstance().beginFrame();
    }

    EMSCRIPTEN_KEEPALIVE
    void irrlicht_orchestrator_end_frame() {
        irr::webgpu::OrchestratorIntegration::getInstance().endFrame();
    }

    EMSCRIPTEN_KEEPALIVE
    int irrlicht_orchestrator_available() {
        return irr::webgpu::OrchestratorIntegration::getInstance().isInitialized() ? 1 : 0;
    }
}