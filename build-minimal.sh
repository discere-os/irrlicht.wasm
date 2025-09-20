#!/bin/bash
set -e

echo "🎮 Building minimal Irrlicht.wasm for testing"
mkdir -p install/wasm

emcc wasm/minimal_irrlicht.cpp \
  source/Irrlicht/CIrrlichtSIMD.cpp \
  -I./include -I./source/Irrlicht \
  -O2 -msimd128 \
  -sMODULARIZE=1 \
  -sEXPORT_ES6=1 \
  -sEXPORT_NAME="IrrlichtModule" \
  -sEXPORTED_FUNCTIONS='["_irrlicht_create_device","_irrlicht_destroy","_irrlicht_get_version","_irrlicht_run","_irrlicht_begin_scene","_irrlicht_end_scene","_irrlicht_draw_all","_irrlicht_add_cube_scene_node","_irrlicht_add_sphere_scene_node","_irrlicht_add_camera","_irrlicht_set_node_position","_irrlicht_set_node_rotation","_irrlicht_set_node_scale","_irrlicht_simd_available","_irrlicht_benchmark_matrix_multiply","_irrlicht_benchmark_vertex_transform","_irrlicht_get_driver_type","_irrlicht_get_driver_name","_irrlicht_print_device_info","_malloc","_free"]' \
  -sEXPORTED_RUNTIME_METHODS='["cwrap","ccall","UTF8ToString"]' \
  -sALLOW_MEMORY_GROWTH=1 \
  -sENVIRONMENT=web,webview,worker \
  -o install/wasm/irrlicht-main.js

echo "✅ Minimal build completed"
echo "📊 Output size: $(du -h install/wasm/irrlicht-main.js | cut -f1)"