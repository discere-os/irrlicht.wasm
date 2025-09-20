/*
 * Minimal Irrlicht.wasm Implementation for Testing
 * Copyright (c) 2002-2012 Nikolaus Gebhardt
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license
 */

#include <emscripten.h>
#include <cstdio>
#include <cstring>

// Minimal Irrlicht API for testing
extern "C" {

EMSCRIPTEN_KEEPALIVE
const char* irrlicht_get_version() {
    return "1.9.0-wasm";
}

EMSCRIPTEN_KEEPALIVE
int irrlicht_create_device(int driverType, int width, int height, int bits, bool fullscreen) {
    printf("🎮 Creating Irrlicht device: driver=%d, size=%dx%d\n", driverType, width, height);
    return 1; // Success
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_destroy() {
    printf("🧹 Destroying Irrlicht device\n");
}

EMSCRIPTEN_KEEPALIVE
bool irrlicht_run() {
    return true;
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_begin_scene(int r, int g, int b, int a) {
    // Minimal implementation
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_end_scene() {
    // Minimal implementation
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_draw_all() {
    // Minimal implementation
}

EMSCRIPTEN_KEEPALIVE
int irrlicht_add_cube_scene_node(float size) {
    printf("📦 Adding cube with size %.2f\n", size);
    return 1; // Mock handle
}

EMSCRIPTEN_KEEPALIVE
int irrlicht_add_sphere_scene_node(float radius, int polyCount) {
    printf("🔴 Adding sphere with radius %.2f, polyCount %d\n", radius, polyCount);
    return 2; // Mock handle
}

EMSCRIPTEN_KEEPALIVE
int irrlicht_add_camera(float px, float py, float pz, float tx, float ty, float tz) {
    printf("📷 Adding camera at (%.2f,%.2f,%.2f) -> (%.2f,%.2f,%.2f)\n", px, py, pz, tx, ty, tz);
    return 1; // Mock handle
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_set_node_position(int node, float x, float y, float z) {
    printf("📍 Setting node %d position to (%.2f,%.2f,%.2f)\n", node, x, y, z);
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_set_node_rotation(int node, float x, float y, float z) {
    printf("🔄 Setting node %d rotation to (%.2f,%.2f,%.2f)\n", node, x, y, z);
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_set_node_scale(int node, float x, float y, float z) {
    printf("📏 Setting node %d scale to (%.2f,%.2f,%.2f)\n", node, x, y, z);
}

// SIMD and benchmark functions are now provided by CIrrlichtSIMD.cpp

EMSCRIPTEN_KEEPALIVE
int irrlicht_get_driver_type() {
    return 0; // EDT_NULL for minimal implementation
}

EMSCRIPTEN_KEEPALIVE
const char* irrlicht_get_driver_name() {
    return "Minimal Test Driver";
}

// Forward declaration for SIMD function from CIrrlichtSIMD.cpp
extern "C" bool irrlicht_simd_available();

EMSCRIPTEN_KEEPALIVE
void irrlicht_print_device_info() {
    printf("\n🎮 Irrlicht.wasm Device Information\n");
    printf("===================================\n");
    printf("Version: %s\n", irrlicht_get_version());
    printf("SIMD Support: %s\n", irrlicht_simd_available() ? "✅" : "❌");
    printf("Build: Minimal test implementation\n");
    printf("Status: Ready for full implementation\n\n");
}

} // extern "C"