/*
 * Test API for Irrlicht.wasm - Mock implementation for testing
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license
 */

#include <emscripten.h>
#include <cstdio>

extern "C" {

// Mock device for testing
static void* mock_device = (void*)0x12345678;
static void* mock_driver = (void*)0x87654321;
static void* mock_scene_manager = (void*)0x13579246;

EMSCRIPTEN_KEEPALIVE
void* irrlicht_create_device(int driverType, int width, int height, int bits, bool fullscreen) {
    // Mock successful device creation
    return mock_device;
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_destroy() {
    // Mock cleanup
}

EMSCRIPTEN_KEEPALIVE
const char* irrlicht_get_version() {
    return "1.9.0-Enhanced";
}

EMSCRIPTEN_KEEPALIVE
void* irrlicht_get_video_driver() {
    return mock_driver;
}

EMSCRIPTEN_KEEPALIVE
void* irrlicht_get_scene_manager() {
    return mock_scene_manager;
}

EMSCRIPTEN_KEEPALIVE
bool irrlicht_run() {
    return true; // Mock always running
}

EMSCRIPTEN_KEEPALIVE
bool irrlicht_begin_scene(unsigned char r, unsigned char g, unsigned char b) {
    return true; // Mock successful begin
}

EMSCRIPTEN_KEEPALIVE
bool irrlicht_end_scene() {
    return true; // Mock successful end
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_draw_all() {
    // Mock draw all
}

EMSCRIPTEN_KEEPALIVE
int irrlicht_get_fps() {
    return 60; // Mock 60 FPS
}

EMSCRIPTEN_KEEPALIVE
int irrlicht_get_primitive_count() {
    return 1000; // Mock primitive count
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_print_device_info() {
    // Mock device info printing
    printf("🎮 Irrlicht Device Info:\n");
    printf("  Driver: WebGPU 1.0 (GPU-Driven Enhanced)\n");
    printf("  Version: 1.9.0-Enhanced\n");
    printf("  Resolution: 800x600\n");
    printf("  SIMD: Enabled\n");
    printf("  GPU-Driven Enhancements: Active\n");
}

// Scene management mock functions
EMSCRIPTEN_KEEPALIVE
void* irrlicht_add_camera(float x, float y, float z, float tx, float ty, float tz) {
    return (void*)0x11111111; // Mock camera node
}

EMSCRIPTEN_KEEPALIVE
void* irrlicht_add_cube_scene_node(float size) {
    return (void*)0x22222222; // Mock cube node
}

EMSCRIPTEN_KEEPALIVE
void* irrlicht_add_sphere_scene_node(float radius, int polyCount) {
    return (void*)0x55555555; // Mock sphere node
}

EMSCRIPTEN_KEEPALIVE
void* irrlicht_get_mesh(const char* filename) {
    return (void*)0x33333333; // Mock mesh
}

EMSCRIPTEN_KEEPALIVE
void* irrlicht_add_mesh_scene_node(void* mesh) {
    return (void*)0x44444444; // Mock mesh scene node
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_set_node_position(void* node, float x, float y, float z) {
    // Mock position setting
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_set_node_rotation(void* node, float x, float y, float z) {
    // Mock rotation setting
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_set_node_scale(void* node, float x, float y, float z) {
    // Mock scale setting
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_set_material_flag(void* node, int flag, bool value) {
    // Mock material flag setting
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_set_material_texture(void* node, int layer, void* texture) {
    // Mock texture setting
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_set_material_type(void* node, int type) {
    // Mock material type setting
}

EMSCRIPTEN_KEEPALIVE
const char* irrlicht_get_driver_name() {
    return "WebGPU 1.0 (GPU-Driven Enhanced)";
}

EMSCRIPTEN_KEEPALIVE
bool irrlicht_dependency_loaded(const char* name) {
    return true; // Mock all dependencies loaded
}

// Include the working SIMD functions from our proven implementation
extern bool irrlicht_simd_available();
extern void irrlicht_matrix_multiply_simd(const float* a, const float* b, float* result);
extern void irrlicht_matrix_multiply_batch_simd(const float* matrices_a, const float* matrices_b, float* results, unsigned int count);
extern void irrlicht_transform_vertices_batch_simd(const float* matrices, const float* vertices, float* results, unsigned int vertex_count, unsigned int matrix_count, const unsigned char* bone_indices, const float* bone_weights);
extern void irrlicht_frustum_cull_batch_simd(const float* frustum_planes, const float* bounding_spheres, unsigned char* visibility_results, unsigned int object_count);

} // extern "C"