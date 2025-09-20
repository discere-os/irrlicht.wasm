#!/bin/bash
set -e

echo "🎮 Building Irrlicht.wasm (simplified)"
mkdir -p install/wasm

echo "📦 Building test version..."

emcc -I./include -I./source/Irrlicht source/Irrlicht/Irrlicht.cpp source/Irrlicht/CNullDriver.cpp source/Irrlicht/CIrrDeviceStub.cpp source/Irrlicht/CLogger.cpp source/Irrlicht/os.cpp wasm/irrlicht_wasm_api.cpp -O2 -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME="IrrlichtModule" -sEXPORTED_FUNCTIONS='["_irrlicht_create_device","_irrlicht_destroy","_irrlicht_get_version","_malloc","_free"]' -sEXPORTED_RUNTIME_METHODS='["cwrap","ccall","UTF8ToString"]' -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web,webview,worker -D_IRR_COMPILE_WITH_SDL_DEVICE_ -o install/wasm/irrlicht-main.js

echo "✅ Build completed"
echo "📊 Output size: $(du -h install/wasm/irrlicht-main.js | cut -f1)"