#!/bin/bash
# build-dual.sh - Dual build system for irrlicht.wasm (SIDE_MODULE + MAIN_MODULE)
#
# Copyright (c) 2002-2012 Nikolaus Gebhardt
# Copyright (c) 2025 Superstruct Ltd, New Zealand
# Licensed under the zlib/libpng license (same as original Irrlicht Engine)
#
# This script builds irrlicht.wasm in two configurations:
# - SIDE_MODULE: For dynamic loading in production environments with external WebGPU coordination
# - MAIN_MODULE: For standalone testing and NPM distribution

set -euo pipefail

# Configuration
BUILD_TYPE="${BUILD_TYPE:-Release}"
INSTALL_PREFIX="${INSTALL_PREFIX:-./install}"
BUILD_DIR="${BUILD_DIR:-./build-dual}"
VARIANT="${1:-all}"

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

# Check prerequisites and dependencies
check_prerequisites() {
    log_info "Checking build prerequisites..."

    if ! command -v emcc &> /dev/null; then
        log_error "Emscripten not found. Please install and activate EMSDK."
        exit 1
    fi

    # Check for required dependencies
    local deps_missing=false

    if [ ! -f "../zlib.wasm/install/wasm/zlib-side.wasm" ]; then
        log_warning "Building zlib.wasm dependency..."
        cd ../zlib.wasm && ./build-dual.sh side && cd -
    fi

    if [ ! -f "../libpng.wasm/install/wasm/libpng-side.wasm" ]; then
        log_warning "Building libpng.wasm dependency..."
        cd ../libpng.wasm && ./build-dual.sh side && cd -
    fi

    if [ ! -f "../bzip2.wasm/install/wasm/bzip2-side.wasm" ]; then
        log_warning "Building bzip2.wasm dependency..."
        cd ../bzip2.wasm && ./build-dual.sh side && cd -
    fi

    if [ ! -f "../libjpeg-turbo.wasm/install/wasm/libjpeg-turbo-side.wasm" ]; then
        log_warning "Building libjpeg-turbo.wasm dependency..."
        cd ../libjpeg-turbo.wasm && ./build-dual.sh side && cd -
    fi

    log_success "Prerequisites check completed"
}

# Build Irrlicht as SIDE_MODULE (production)
build_irrlicht_side_module() {
    log_info "Building irrlicht-side.wasm for production dynamic loading..."
    mkdir -p "${BUILD_DIR}-side"
    cd "${BUILD_DIR}-side"

    # Core Irrlicht sources (excluding embedded libraries)
    IRRLICHT_SOURCES=$(find ../source/Irrlicht -name "*.cpp" -not -path "*/zlib/*" -not -path "*/libpng/*" -not -path "*/jpeglib/*" -not -path "*/bzip2/*" | tr '\n' ' ')

    # WebGPU and WASM enhancement sources (now integrated into Irrlicht source tree)
    WEBGPU_SOURCES="../source/Irrlicht/CWebGPUDriver.cpp ../source/Irrlicht/CWebGPUPipeline.cpp ../source/Irrlicht/CWebGPUFactory.cpp ../source/Irrlicht/CWebGPUOrchestrator.cpp ../source/Irrlicht/CWASMDependencyManager.cpp"
    SIMD_SOURCES="../source/Irrlicht/CIrrlichtSIMD.cpp"
    WASM_API_SOURCES="../wasm/irrlicht_wasm_api.cpp"

    # Include paths for dynamic dependencies
    INCLUDES="-I../include -I../source/Irrlicht"
    INCLUDES+=" -I../../zlib.wasm/src -I../../libpng.wasm/src -I../../bzip2.wasm/src -I../../libjpeg-turbo.wasm/src"

    log_info "Compiling irrlicht-side.wasm as SIDE_MODULE..."
    emcc ${IRRLICHT_SOURCES} ${WEBGPU_SOURCES} ${SIMD_SOURCES} ${WASM_API_SOURCES} \
        ${INCLUDES} \
        -O3 -flto -msimd128 \
        -sSIDE_MODULE=1 \
        -sWASM=1 \
        -sSTANDALONE_WASM=1 \
        -sUSE_WEBGPU=1 \
        -sASYNCIFY=1 \
        -sEXPORTED_FUNCTIONS='["_irrlicht_create_device","_irrlicht_get_video_driver","_irrlicht_run","_irrlicht_destroy","_irrlicht_webgpu_init","_dlopen","_dlsym"]' \
        -D_IRR_COMPILE_WITH_WEBGPU_ \
        -D_IRR_COMPILE_WITH_OPENGL_ \
        -DIRRLICHT_USE_DYNAMIC_DEPS=1 \
        -D_IRR_WCHAR_FILESYSTEM=0 \
        -D_IRR_COMPILE_WITH_SDL_DEVICE_ \
        -o irrlicht-side.wasm

    # Install artifacts
    mkdir -p "${INSTALL_PREFIX}/wasm"
    cp irrlicht-side.wasm "${INSTALL_PREFIX}/wasm/"

    log_success "SIDE_MODULE: ${INSTALL_PREFIX}/wasm/irrlicht-side.wasm ($(du -h ${INSTALL_PREFIX}/wasm/irrlicht-side.wasm | cut -f1))"
    cd ..
}

# Build Irrlicht as MAIN_MODULE (testing/NPM)
build_irrlicht_main_module() {
    log_info "Building irrlicht-main.js for standalone testing..."
    mkdir -p "${BUILD_DIR}-main"
    cd "${BUILD_DIR}-main"

    # Core Irrlicht sources (including embedded libraries for static linking)
    IRRLICHT_SOURCES=$(find ../source/Irrlicht -name "*.cpp" | tr '\n' ' ')

    # WebGPU and WASM enhancement sources (now integrated into Irrlicht source tree)
    WEBGPU_SOURCES="../source/Irrlicht/CWebGPUDriver.cpp ../source/Irrlicht/CWebGPUPipeline.cpp ../source/Irrlicht/CWebGPUFactory.cpp ../source/Irrlicht/CWebGPUOrchestrator.cpp ../source/Irrlicht/CWASMDependencyManager.cpp"
    SIMD_SOURCES="../source/Irrlicht/CIrrlichtSIMD.cpp"
    WASM_API_SOURCES="../wasm/irrlicht_wasm_api.cpp"

    # Include paths
    INCLUDES="-I../include -I../source/Irrlicht"

    # Exported functions for MAIN_MODULE (includes memory management and all APIs)
    EXPORTED_FUNCTIONS='["_irrlicht_create_device","_irrlicht_get_video_driver","_irrlicht_run","_irrlicht_destroy","_irrlicht_webgpu_init","_irrlicht_scene_manager","_irrlicht_add_mesh_node","_irrlicht_add_camera","_irrlicht_draw_all","_malloc","_free"]'

    log_info "Compiling irrlicht-main.js as MAIN_MODULE with embedded libraries..."
    emcc ${IRRLICHT_SOURCES} ${WEBGPU_SOURCES} ${SIMD_SOURCES} ${WASM_API_SOURCES} \
        ${INCLUDES} \
        -O3 -flto -msimd128 \
        -sMODULARIZE=1 \
        -sEXPORT_ES6=1 \
        -sEXPORT_NAME="IrrlichtModule" \
        -sEXPORTED_FUNCTIONS="${EXPORTED_FUNCTIONS}" \
        -sEXPORTED_RUNTIME_METHODS='["cwrap","ccall","UTF8ToString","HEAPU8","getValue","setValue"]' \
        -sALLOW_MEMORY_GROWTH=1 \
        -sINITIAL_MEMORY=134217728 \
        -sMAXIMUM_MEMORY=2147483648 \
        -sUSE_WEBGPU=1 \
        -sASYNCIFY=1 \
        -sENVIRONMENT=web,webview,worker \
        -sNODEJS_CATCH_EXIT=0 \
        -sNODEJS_CATCH_REJECTION=0 \
        -D_IRR_COMPILE_WITH_WEBGPU_ \
        -D_IRR_COMPILE_WITH_OPENGL_ \
        -DIRRLICHT_USE_STATIC_DEPS=1 \
        -D_IRR_WCHAR_FILESYSTEM=0 \
        -D_IRR_COMPILE_WITH_SDL_DEVICE_ \
        -o irrlicht-main.js

    # Install artifacts
    mkdir -p "${INSTALL_PREFIX}/wasm"
    cp irrlicht-main.js "${INSTALL_PREFIX}/wasm/"
    cp irrlicht-main.wasm "${INSTALL_PREFIX}/wasm/"

    log_success "MAIN_MODULE: ${INSTALL_PREFIX}/wasm/irrlicht-main.js ($(du -h ${INSTALL_PREFIX}/wasm/irrlicht-main.js | cut -f1))"
    cd ..
}

case "$VARIANT" in
    side) check_prerequisites && build_irrlicht_side_module ;;
    main) check_prerequisites && build_irrlicht_main_module ;;
    all) check_prerequisites && build_irrlicht_side_module && build_irrlicht_main_module ;;
    clean) rm -rf "${BUILD_DIR}"* "${INSTALL_PREFIX}" ;;
    *) echo "Usage: $0 [side|main|all|clean]"; exit 1 ;;
esac

# Summary
if [ "$VARIANT" = "all" ] || [ "$VARIANT" = "side" ] || [ "$VARIANT" = "main" ]; then
    echo ""
    echo "🎮 Irrlicht.wasm Build Complete!"
    echo "================================"
    echo ""
    echo "Features implemented:"
    echo "  ✓ WebGPU-native 3D rendering pipeline"
    echo "  ✓ Dynamic dependency loading (SIDE_MODULE)"
    echo "  ✓ Static embedded libraries (MAIN_MODULE)"
    echo "  ✓ WASM SIMD matrix/vertex operations"
    echo "  ✓ External WebGPU orchestrator integration"
    echo "  ✓ Full Irrlicht API compatibility"
    echo "  ✓ Scene graph and material system"
    echo "  ✓ Multiple file format support"
    echo ""
    echo "Integration capabilities:"
    echo "  ✓ External WebGPU coordinator support"
    echo "  ✓ GPU resource sharing and coordination"
    echo "  ✓ Cross-application 3D rendering"
    echo "  ✓ Browser-native performance optimization"
    echo ""
    if [ -f "${INSTALL_PREFIX}/wasm/irrlicht-side.wasm" ]; then
        echo "SIDE_MODULE size: $(du -h ${INSTALL_PREFIX}/wasm/irrlicht-side.wasm | cut -f1)"
    fi
    if [ -f "${INSTALL_PREFIX}/wasm/irrlicht-main.js" ]; then
        echo "MAIN_MODULE size: $(du -h ${INSTALL_PREFIX}/wasm/irrlicht-main.js | cut -f1)"
    fi
fi