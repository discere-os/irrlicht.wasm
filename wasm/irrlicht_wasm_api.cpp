/*
 * Irrlicht.wasm C API Implementation
 * Copyright (c) 2002-2012 Nikolaus Gebhardt
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license
 *
 * Complete C API wrapper for Irrlicht Engine with WebGPU support
 */

#include <emscripten.h>
#include <emscripten/bind.h>
#include "irrlicht.h"
#include "CWASMDependencyManager.h"

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;
using namespace io;
using namespace gui;

// Global Irrlicht device pointer
static IrrlichtDevice* g_device = nullptr;

extern "C" {

// Device management
EMSCRIPTEN_KEEPALIVE
IrrlichtDevice* irrlicht_create_device(int driverType, int width, int height, int bits, bool fullscreen) {
    if (g_device) {
        g_device->drop();
        g_device = nullptr;
    }

    // Initialize dependencies first
    if (!irrlicht_init_dependencies()) {
        printf("⚠️  Warning: Some dependencies failed to load\n");
    }

    SIrrlichtCreationParameters params;
    params.DriverType = (E_DRIVER_TYPE)driverType;
    params.WindowSize = dimension2d<u32>(width, height);
    params.Bits = bits;
    params.Fullscreen = fullscreen;
    params.Stencilbuffer = true;
    params.Vsync = false;
    params.EventReceiver = nullptr;

    g_device = createDeviceEx(params);
    if (!g_device) {
        printf("❌ Failed to create Irrlicht device with driver type %d\n", driverType);
        return nullptr;
    }

    printf("✅ Irrlicht device created successfully with %s driver\n",
           DRIVER_TYPE_NAMES[driverType]);

    return g_device;
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_destroy() {
    if (g_device) {
        g_device->drop();
        g_device = nullptr;
    }
    irrlicht_cleanup_dependencies();
    printf("🧹 Irrlicht device destroyed and dependencies cleaned up\n");
}

EMSCRIPTEN_KEEPALIVE
bool irrlicht_run() {
    if (!g_device) return false;
    return g_device->run();
}

EMSCRIPTEN_KEEPALIVE
IVideoDriver* irrlicht_get_video_driver() {
    if (!g_device) return nullptr;
    return g_device->getVideoDriver();
}

EMSCRIPTEN_KEEPALIVE
ISceneManager* irrlicht_get_scene_manager() {
    if (!g_device) return nullptr;
    return g_device->getSceneManager();
}

EMSCRIPTEN_KEEPALIVE
IGUIEnvironment* irrlicht_get_gui_environment() {
    if (!g_device) return nullptr;
    return g_device->getGUIEnvironment();
}

// Scene management
EMSCRIPTEN_KEEPALIVE
ICameraSceneNode* irrlicht_add_camera_fps(float moveSpeed, float rotateSpeed, int keyMapArray, int keyMapSize) {
    if (!g_device) return nullptr;
    ISceneManager* smgr = g_device->getSceneManager();
    return smgr->addCameraSceneNodeFPS(nullptr, moveSpeed, rotateSpeed, -1,
                                      (SKeyMap*)keyMapArray, keyMapSize);
}

EMSCRIPTEN_KEEPALIVE
ICameraSceneNode* irrlicht_add_camera(float posX, float posY, float posZ,
                                     float targetX, float targetY, float targetZ) {
    if (!g_device) return nullptr;
    ISceneManager* smgr = g_device->getSceneManager();
    return smgr->addCameraSceneNode(nullptr,
                                   vector3df(posX, posY, posZ),
                                   vector3df(targetX, targetY, targetZ));
}

EMSCRIPTEN_KEEPALIVE
IMeshSceneNode* irrlicht_add_cube_scene_node(float size) {
    if (!g_device) return nullptr;
    ISceneManager* smgr = g_device->getSceneManager();
    IMesh* mesh = smgr->getGeometryCreator()->createCubeMesh(vector3df(size, size, size));
    return smgr->addMeshSceneNode(mesh);
}

EMSCRIPTEN_KEEPALIVE
IMeshSceneNode* irrlicht_add_sphere_scene_node(float radius, int polyCount) {
    if (!g_device) return nullptr;
    ISceneManager* smgr = g_device->getSceneManager();
    IMesh* mesh = smgr->getGeometryCreator()->createSphereMesh(radius, polyCount, polyCount);
    return smgr->addMeshSceneNode(mesh);
}

EMSCRIPTEN_KEEPALIVE
IAnimatedMesh* irrlicht_get_mesh(const char* filename) {
    if (!g_device) return nullptr;
    ISceneManager* smgr = g_device->getSceneManager();
    return smgr->getMesh(filename);
}

EMSCRIPTEN_KEEPALIVE
IMeshSceneNode* irrlicht_add_mesh_scene_node(IAnimatedMesh* mesh, ISceneNode* parent) {
    if (!g_device || !mesh) return nullptr;
    ISceneManager* smgr = g_device->getSceneManager();
    return smgr->addMeshSceneNode(mesh, parent);
}

// Rendering control
EMSCRIPTEN_KEEPALIVE
void irrlicht_begin_scene(int clearFlag, int clearColorR, int clearColorG, int clearColorB, int clearColorA) {
    if (!g_device) return;
    IVideoDriver* driver = g_device->getVideoDriver();
    SColor clearColor(clearColorA, clearColorR, clearColorG, clearColorB);
    driver->beginScene(clearFlag, clearColor);
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_end_scene() {
    if (!g_device) return;
    g_device->getVideoDriver()->endScene();
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_draw_all() {
    if (!g_device) return;
    g_device->getSceneManager()->drawAll();
    g_device->getGUIEnvironment()->drawAll();
}

// Texture management
EMSCRIPTEN_KEEPALIVE
ITexture* irrlicht_get_texture(const char* filename) {
    if (!g_device) return nullptr;
    IVideoDriver* driver = g_device->getVideoDriver();
    return driver->getTexture(filename);
}

EMSCRIPTEN_KEEPALIVE
ITexture* irrlicht_create_texture_from_data(int width, int height, void* data, int format) {
    if (!g_device || !data) return nullptr;
    IVideoDriver* driver = g_device->getVideoDriver();

    // Create image from data
    IImage* image = driver->createImageFromData((ECOLOR_FORMAT)format,
                                               dimension2d<u32>(width, height),
                                               data, true, false);
    if (!image) return nullptr;

    ITexture* texture = driver->addTexture("runtime_texture", image);
    image->drop();
    return texture;
}

// Material management
EMSCRIPTEN_KEEPALIVE
void irrlicht_set_material_texture(IMeshSceneNode* node, int layer, ITexture* texture) {
    if (!node || !texture) return;
    node->setMaterialTexture(layer, texture);
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_set_material_type(IMeshSceneNode* node, int materialType) {
    if (!node) return;
    node->setMaterialType((E_MATERIAL_TYPE)materialType);
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_set_material_flag(IMeshSceneNode* node, int flag, bool value) {
    if (!node) return;
    node->setMaterialFlag((E_MATERIAL_FLAG)flag, value);
}

// Scene node manipulation
EMSCRIPTEN_KEEPALIVE
void irrlicht_set_node_position(ISceneNode* node, float x, float y, float z) {
    if (!node) return;
    node->setPosition(vector3df(x, y, z));
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_set_node_rotation(ISceneNode* node, float x, float y, float z) {
    if (!node) return;
    node->setRotation(vector3df(x, y, z));
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_set_node_scale(ISceneNode* node, float x, float y, float z) {
    if (!node) return;
    node->setScale(vector3df(x, y, z));
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_get_node_position(ISceneNode* node, float* x, float* y, float* z) {
    if (!node || !x || !y || !z) return;
    vector3df pos = node->getPosition();
    *x = pos.X;
    *y = pos.Y;
    *z = pos.Z;
}

// Animation
EMSCRIPTEN_KEEPALIVE
ISceneNodeAnimator* irrlicht_create_fly_circle_animator(float centerX, float centerY, float centerZ,
                                                       float radius, float speed) {
    if (!g_device) return nullptr;
    ISceneManager* smgr = g_device->getSceneManager();
    return smgr->createFlyCircleAnimator(vector3df(centerX, centerY, centerZ), radius, speed);
}

EMSCRIPTEN_KEEPALIVE
ISceneNodeAnimator* irrlicht_create_rotation_animator(float speedX, float speedY, float speedZ) {
    if (!g_device) return nullptr;
    ISceneManager* smgr = g_device->getSceneManager();
    return smgr->createRotationAnimator(vector3df(speedX, speedY, speedZ));
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_add_animator(ISceneNode* node, ISceneNodeAnimator* animator) {
    if (!node || !animator) return;
    node->addAnimator(animator);
    animator->drop(); // Irrlicht manages the reference
}

// Lighting
EMSCRIPTEN_KEEPALIVE
ILightSceneNode* irrlicht_add_light(float posX, float posY, float posZ,
                                   float colorR, float colorG, float colorB,
                                   float radius) {
    if (!g_device) return nullptr;
    ISceneManager* smgr = g_device->getSceneManager();
    SColorf color(colorR, colorG, colorB, 1.0f);
    return smgr->addLightSceneNode(nullptr, vector3df(posX, posY, posZ), color, radius);
}

// Mesh manipulation
EMSCRIPTEN_KEEPALIVE
void irrlicht_mesh_set_material_flag(IMesh* mesh, int flag, bool value) {
    if (!mesh) return;
    for (u32 i = 0; i < mesh->getMeshBufferCount(); ++i) {
        mesh->getMeshBuffer(i)->getMaterial().setFlag((E_MATERIAL_FLAG)flag, value);
    }
}

// File system
EMSCRIPTEN_KEEPALIVE
bool irrlicht_file_exists(const char* filename) {
    if (!g_device) return false;
    IFileSystem* fs = g_device->getFileSystem();
    return fs->existFile(filename);
}

// Utility functions
EMSCRIPTEN_KEEPALIVE
const char* irrlicht_get_version() {
    return IRRLICHT_SDK_VERSION;
}

EMSCRIPTEN_KEEPALIVE
int irrlicht_get_driver_type() {
    if (!g_device) return EDT_NULL;
    return g_device->getVideoDriver()->getDriverType();
}

EMSCRIPTEN_KEEPALIVE
const char* irrlicht_get_driver_name() {
    if (!g_device) return "None";
    E_DRIVER_TYPE type = g_device->getVideoDriver()->getDriverType();
    return DRIVER_TYPE_NAMES[type];
}

EMSCRIPTEN_KEEPALIVE
bool irrlicht_is_webgpu_driver() {
    if (!g_device) return false;
    return g_device->getVideoDriver()->getDriverType() == EDT_WEBGPU;
}

// Performance monitoring
EMSCRIPTEN_KEEPALIVE
int irrlicht_get_fps() {
    if (!g_device) return 0;
    return g_device->getVideoDriver()->getFPS();
}

EMSCRIPTEN_KEEPALIVE
int irrlicht_get_primitive_count() {
    if (!g_device) return 0;
    return g_device->getVideoDriver()->getPrimitiveCountDrawn();
}

// WebGPU-specific functions
EMSCRIPTEN_KEEPALIVE
bool irrlicht_webgpu_init() {
    if (!g_device) return false;

    // Initialize orchestrator integration (placeholder)
    // irrlicht_init_orchestrator();

    // Additional WebGPU initialization if needed
    IVideoDriver* driver = g_device->getVideoDriver();
    if (driver->getDriverType() == EDT_WEBGPU) {
        printf("🎮 WebGPU driver initialized successfully\n");
        return true;
    }

    return false;
}

EMSCRIPTEN_KEEPALIVE
bool irrlicht_webgpu_orchestrator_available() {
    return false; // Placeholder for orchestrator availability
}

// Error handling
EMSCRIPTEN_KEEPALIVE
const char* irrlicht_get_last_error() {
    if (!g_device) return "No device";
    // Could implement error logging system
    return "No error";
}

// Debug and diagnostics
EMSCRIPTEN_KEEPALIVE
void irrlicht_print_device_info() {
    if (!g_device) {
        printf("❌ No Irrlicht device available\n");
        return;
    }

    printf("\n🎮 Irrlicht Engine Device Information\n");
    printf("=====================================\n");
    printf("SDK Version: %s\n", IRRLICHT_SDK_VERSION);
    printf("Driver Type: %s\n", irrlicht_get_driver_name());
    printf("Screen Size: %dx%d\n",
           g_device->getVideoDriver()->getScreenSize().Width,
           g_device->getVideoDriver()->getScreenSize().Height);
    printf("Color Format: %d-bit\n", g_device->getCreationParameters().Bits);
    printf("Fullscreen: %s\n", g_device->getCreationParameters().Fullscreen ? "Yes" : "No");
    printf("Stencil Buffer: %s\n", g_device->getCreationParameters().Stencilbuffer ? "Yes" : "No");

    if (irrlicht_is_webgpu_driver()) {
        printf("WebGPU Orchestrator: %s\n",
               irrlicht_webgpu_orchestrator_available() ? "Available" : "Standalone");
    }

    printf("Dependencies loaded:\n");
    printf("  - zlib: %s\n", irrlicht_dependency_loaded("zlib") ? "✓" : "✗");
    printf("  - libpng: %s\n", irrlicht_dependency_loaded("libpng") ? "✓" : "✗");
    printf("  - libjpeg: %s\n", irrlicht_dependency_loaded("libjpeg") ? "✓" : "✗");
    printf("  - bzip2: %s\n", irrlicht_dependency_loaded("bzip2") ? "✓" : "✗");
    printf("\n");
}

} // extern "C"

// Emscripten bindings for C++ class access
EMSCRIPTEN_BINDINGS(irrlicht_bindings) {
    // Basic types
    emscripten::value_object<dimension2d<u32>>("dimension2du")
        .field("Width", &dimension2d<u32>::Width)
        .field("Height", &dimension2d<u32>::Height);

    emscripten::value_object<vector3df>("vector3df")
        .field("X", &vector3df::X)
        .field("Y", &vector3df::Y)
        .field("Z", &vector3df::Z);

    emscripten::value_object<SColor>("SColor")
        .field("color", &SColor::color);

    // Driver types
    emscripten::enum_<E_DRIVER_TYPE>("E_DRIVER_TYPE")
        .value("EDT_NULL", EDT_NULL)
        .value("EDT_SOFTWARE", EDT_SOFTWARE)
        .value("EDT_BURNINGSVIDEO", EDT_BURNINGSVIDEO)
        .value("EDT_DIRECT3D9", EDT_DIRECT3D9)
        .value("EDT_OPENGL", EDT_OPENGL)
        .value("EDT_WEBGPU", EDT_WEBGPU);

    // Material types
    emscripten::enum_<E_MATERIAL_TYPE>("E_MATERIAL_TYPE")
        .value("EMT_SOLID", EMT_SOLID)
        .value("EMT_LIGHTMAP", EMT_LIGHTMAP)
        .value("EMT_TRANSPARENT_ADD_COLOR", EMT_TRANSPARENT_ADD_COLOR)
        .value("EMT_TRANSPARENT_ALPHA_CHANNEL", EMT_TRANSPARENT_ALPHA_CHANNEL);

    // Scene node interfaces
    emscripten::class_<ISceneNode>("ISceneNode")
        .function("setPosition", emscripten::select_overload<void(const vector3df&)>(&ISceneNode::setPosition))
        .function("setRotation", emscripten::select_overload<void(const vector3df&)>(&ISceneNode::setRotation))
        .function("setScale", emscripten::select_overload<void(const vector3df&)>(&ISceneNode::setScale))
        .function("getPosition", &ISceneNode::getPosition)
        .function("getRotation", &ISceneNode::getRotation)
        .function("getScale", &ISceneNode::getScale)
        .function("setVisible", &ISceneNode::setVisible)
        .function("isVisible", &ISceneNode::isVisible);

    emscripten::class_<IMeshSceneNode, emscripten::base<ISceneNode>>("IMeshSceneNode")
        .function("setMaterialFlag", &IMeshSceneNode::setMaterialFlag)
        .function("setMaterialTexture", &IMeshSceneNode::setMaterialTexture)
        .function("setMaterialType", &IMeshSceneNode::setMaterialType);

    emscripten::class_<ICameraSceneNode, emscripten::base<ISceneNode>>("ICameraSceneNode")
        .function("setTarget", &ICameraSceneNode::setTarget)
        .function("getTarget", &ICameraSceneNode::getTarget)
        .function("setFOV", &ICameraSceneNode::setFOV)
        .function("getFOV", &ICameraSceneNode::getFOV);

    // Device interfaces
    emscripten::class_<IrrlichtDevice>("IrrlichtDevice")
        .function("run", &IrrlichtDevice::run)
        .function("getVideoDriver", &IrrlichtDevice::getVideoDriver, emscripten::return_value_policy::reference())
        .function("getSceneManager", &IrrlichtDevice::getSceneManager, emscripten::return_value_policy::reference())
        .function("getGUIEnvironment", &IrrlichtDevice::getGUIEnvironment, emscripten::return_value_policy::reference());

    // Video driver interface
    emscripten::class_<IVideoDriver>("IVideoDriver")
        .function("beginScene", emscripten::select_overload<bool()>(&IVideoDriver::beginScene))
        .function("endScene", &IVideoDriver::endScene)
        .function("getDriverType", &IVideoDriver::getDriverType)
        .function("getScreenSize", &IVideoDriver::getScreenSize)
        .function("getFPS", &IVideoDriver::getFPS);

    // Scene manager interface
    emscripten::class_<ISceneManager>("ISceneManager")
        .function("drawAll", &ISceneManager::drawAll)
        .function("addCameraSceneNode", emscripten::select_overload<ICameraSceneNode*(ISceneNode*, const vector3df&, const vector3df&, s32, bool)>(&ISceneManager::addCameraSceneNode), emscripten::return_value_policy::reference())
        .function("addMeshSceneNode", emscripten::select_overload<IMeshSceneNode*(IMesh*, ISceneNode*, s32, const vector3df&, const vector3df&, const vector3df&)>(&ISceneManager::addMeshSceneNode), emscripten::return_value_policy::reference());
}