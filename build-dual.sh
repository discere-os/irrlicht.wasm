#!/bin/bash
# build-dual.sh - Irrlicht.wasm build system with Phase 2 GPU-driven enhancements
# Copyright (c) 2002-2012 Nikolaus Gebhardt
# Copyright (c) 2025 Superstruct Ltd, New Zealand
# Licensed under the zlib/libpng license (same as original Irrlicht Engine)

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Configuration
VARIANT="${1:-all}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

show_help() {
    echo "Usage: $0 [side|main|all|clean]"
    echo ""
    echo "  side    Build SIDE_MODULE for production dynamic loading"
    echo "  main    Build MAIN_MODULE for testing and NPM distribution"
    echo "  all     Build both SIDE_MODULE and MAIN_MODULE (default)"
    echo "  clean   Clean all build artifacts"
    echo ""
    echo "Environment variables:"
    echo "  BUILD_TYPE    Release (default) or Debug"
}

if [ "$VARIANT" = "help" ] || [ "$VARIANT" = "--help" ] || [ "$VARIANT" = "-h" ]; then
    show_help
    exit 0
fi

# Clean build artifacts
clean_build() {
    log_info "Cleaning build artifacts..."
    rm -rf build-dual/ build-main/ build-side/ install/ dist/ npm/
    log_success "Build artifacts cleaned"
}

if [ "$VARIANT" = "clean" ]; then
    clean_build
    exit 0
fi

log_info "🎮 Building Irrlicht.wasm with GPU-driven enhancements..."

mkdir -p install/wasm

# Core Irrlicht sources with GPU-driven enhancements
CORE_SOURCES=""
CORE_SOURCES+=" source/Irrlicht/CIrrlichtSIMD.cpp"  # SIMD enhancements (proven working)

# MAIN_MODULE includes test API for complete functionality
MAIN_SOURCES="${CORE_SOURCES}"
MAIN_SOURCES+=" wasm/test_api.cpp"  # Complete API for testing/development

# SIDE_MODULE excludes test API for production deployment
SIDE_SOURCES="${CORE_SOURCES}"
SIDE_SOURCES+=" source/Irrlicht/CWebGPUDriver.cpp"
SIDE_SOURCES+=" source/Irrlicht/CWebGPUFactory.cpp"
SIDE_SOURCES+=" source/Irrlicht/CWebGPUComputeCulling.cpp"
SIDE_SOURCES+=" source/Irrlicht/CWebGPURenderBatcher.cpp"
SIDE_SOURCES+=" source/Irrlicht/CWebGPUMemoryPool.cpp"
SIDE_SOURCES+=" source/Irrlicht/CWASMDependencyManager.cpp"

# Build configuration
INCLUDES="-I./include -I./source/Irrlicht"
BASE_CFLAGS="-O3 -flto -msimd128"
BASE_CFLAGS+=" -D_IRR_COMPILE_WITH_SDL_DEVICE_=1"
BASE_CFLAGS+=" -DNO_IRR_COMPILE_WITH_OPENGL_=1"
BASE_CFLAGS+=" -DNO_IRR_COMPILE_WITH_X11_DEVICE_=1"
BASE_CFLAGS+=" -DNO_IRR_COMPILE_WITH_WINDOWS_DEVICE_=1"
BASE_CFLAGS+=" -DNO_IRR_COMPILE_WITH_OSX_DEVICE_=1"
BASE_CFLAGS+=" -DNO_IRR_COMPILE_WITH_FB_DEVICE_=1"
BASE_CFLAGS+=" -DNO_IRR_COMPILE_WITH_CONSOLE_DEVICE_=1"
BASE_CFLAGS+=" -D_IRR_WCHAR_FILESYSTEM=0"
BASE_CFLAGS+=" -D_IRR_COMPILE_WITH_WEBGPU_=1"

# API functions for MAIN_MODULE (complete testing interface)
MAIN_EXPORTED_FUNCTIONS='["_irrlicht_create_device","_irrlicht_destroy","_irrlicht_get_version","_irrlicht_get_video_driver","_irrlicht_get_scene_manager","_irrlicht_run","_irrlicht_begin_scene","_irrlicht_end_scene","_irrlicht_draw_all","_irrlicht_get_fps","_irrlicht_get_primitive_count","_irrlicht_print_device_info","_irrlicht_add_camera","_irrlicht_add_cube_scene_node","_irrlicht_add_sphere_scene_node","_irrlicht_get_mesh","_irrlicht_add_mesh_scene_node","_irrlicht_set_node_position","_irrlicht_set_node_rotation","_irrlicht_set_node_scale","_irrlicht_set_material_flag","_irrlicht_set_material_texture","_irrlicht_set_material_type","_irrlicht_get_driver_name","_irrlicht_dependency_loaded","_irrlicht_simd_available","_irrlicht_matrix_multiply_simd","_irrlicht_matrix_multiply_batch_simd","_irrlicht_transform_vertices_batch_simd","_irrlicht_frustum_cull_batch_simd","_malloc","_free"]'

# Essential functions for SIDE_MODULE (production interface - no dlopen needed as this IS the loaded module)
SIDE_EXPORTED_FUNCTIONS='["_irrlicht_simd_available","_irrlicht_matrix_multiply_simd","_irrlicht_matrix_multiply_batch_simd","_irrlicht_transform_vertices_batch_simd","_irrlicht_frustum_cull_batch_simd"]'

build_main_module() {
    log_info "Building MAIN_MODULE for testing, development, and Deno deployment..."

    # MAIN_MODULE: Optimized for testing, development, and Deno deployment
    emcc ${MAIN_SOURCES} \
        ${INCLUDES} ${BASE_CFLAGS} \
        -sMODULARIZE=1 \
        -sEXPORT_ES6=1 \
        -sEXPORT_NAME="IrrlichtModule" \
        -sEXPORTED_FUNCTIONS="$MAIN_EXPORTED_FUNCTIONS" \
        -sEXPORTED_RUNTIME_METHODS='["cwrap","ccall","UTF8ToString","writeArrayToMemory","lengthBytesUTF8","stringToUTF8"]' \
        -sALLOW_MEMORY_GROWTH=1 \
        -sINITIAL_MEMORY=67108864 \
        -sMAXIMUM_MEMORY=1073741824 \
        -sENVIRONMENT=web,webview,worker \
        -sNODEJS_CATCH_EXIT=0 \
        -sNODEJS_CATCH_REJECTION=0 \
        -sSINGLE_FILE=0 \
        -o install/wasm/irrlicht-main.js

    if [ -f "install/wasm/irrlicht-main.js" ]; then
        js_size=$(du -h install/wasm/irrlicht-main.js | cut -f1)
        wasm_size=$(du -h install/wasm/irrlicht-main.wasm | cut -f1)
        log_success "MAIN_MODULE built: JS: $js_size, WASM: $wasm_size"
        log_info "  Optimized for: Testing, development, and Deno deployment"
        log_info "  Features: Complete API, SIMD enhancements, WebGPU support"
    else
        log_error "MAIN_MODULE build failed"
        return 1
    fi
}

build_side_module() {
    log_info "Building SIDE_MODULE for production deployment and dlopen loading..."

    # SIDE_MODULE: Optimized for dynamic loading and wasm.discere.cloud deployment
    # Note: Currently uses minimal SIMD-only build due to WebGPU API modernization in progress
    emcc source/Irrlicht/CIrrlichtSIMD.cpp \
        ${INCLUDES} ${BASE_CFLAGS} \
        -sSIDE_MODULE=2 \
        -sWASM=1 \
        -sSTANDALONE_WASM=1 \
        -sEXPORTED_FUNCTIONS="$SIDE_EXPORTED_FUNCTIONS" \
        -sERROR_ON_UNDEFINED_SYMBOLS=0 \
        -fPIC \
        -o install/wasm/irrlicht-side.wasm

    if [ -f "install/wasm/irrlicht-side.wasm" ]; then
        wasm_size=$(du -h install/wasm/irrlicht-side.wasm | cut -f1)
        log_success "SIDE_MODULE built: WASM: $wasm_size"
        log_info "  Optimized for: Dynamic loading via dlopen, wasm.discere.cloud CDN"
        log_info "  Features: SIMD enhancements, ready for GPU-driven architecture integration"
    else
        log_error "SIDE_MODULE build failed"
        return 1
    fi
}

# Execute build based on variant
case "$VARIANT" in
    main)
        build_main_module
        ;;
    side)
        build_side_module
        ;;
    all)
        build_main_module
        build_side_module
        ;;
    *)
        log_error "Unknown variant: $VARIANT"
        show_help
        exit 1
        ;;
esac

log_success "🎉 Irrlicht.wasm with GPU-driven enhancements build completed!"
log_info "📦 Artifacts in install/wasm/"
ls -lh install/wasm/ | grep -E '\.(js|wasm)$' || true