/*
 * Simple C API for Irrlicht.wasm testing
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license
 */

#include <emscripten.h>
#include "irrlicht.h"

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;

// Global device for simple API
static IrrlichtDevice* g_device = nullptr;

extern "C" {

EMSCRIPTEN_KEEPALIVE
void* irrlicht_create_device(int driverType, int width, int height, int bits, bool fullscreen) {
    if (g_device) {
        g_device->drop();
        g_device = nullptr;
    }

    SIrrlichtCreationParameters params;
    params.DriverType = (E_DRIVER_TYPE)driverType;
    params.WindowSize = dimension2d<u32>(width, height);
    params.Bits = bits;
    params.Fullscreen = fullscreen;
    params.DeviceType = EIDT_SDL;

    g_device = createDevice(params.DriverType, params.WindowSize, params.Bits,
                           params.Fullscreen, false, false, nullptr);

    return g_device;
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_destroy() {
    if (g_device) {
        g_device->drop();
        g_device = nullptr;
    }
}

EMSCRIPTEN_KEEPALIVE
const char* irrlicht_get_version() {
    return IRRLICHT_SDK_VERSION;
}

EMSCRIPTEN_KEEPALIVE
void* irrlicht_get_video_driver() {
    if (!g_device) return nullptr;
    return g_device->getVideoDriver();
}

EMSCRIPTEN_KEEPALIVE
void* irrlicht_get_scene_manager() {
    if (!g_device) return nullptr;
    return g_device->getSceneManager();
}

EMSCRIPTEN_KEEPALIVE
bool irrlicht_run() {
    if (!g_device) return false;
    return g_device->run();
}

EMSCRIPTEN_KEEPALIVE
bool irrlicht_begin_scene(unsigned char r, unsigned char g, unsigned char b) {
    if (!g_device) return false;
    IVideoDriver* driver = g_device->getVideoDriver();
    return driver->beginScene(true, true, SColor(255, r, g, b));
}

EMSCRIPTEN_KEEPALIVE
bool irrlicht_end_scene() {
    if (!g_device) return false;
    IVideoDriver* driver = g_device->getVideoDriver();
    return driver->endScene();
}

EMSCRIPTEN_KEEPALIVE
void irrlicht_draw_all() {
    if (!g_device) return;
    ISceneManager* smgr = g_device->getSceneManager();
    smgr->drawAll();
}

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

} // extern "C"