/*
 * WebGPU Driver Factory Implementation
 * Copyright (c) 2002-2012 Nikolaus Gebhardt
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license
 */

#include "CWebGPUDriver.h"
#include "CIrrDeviceSDL.h"
#include "os.h"

#ifdef _IRR_COMPILE_WITH_WEBGPU_

namespace irr {
namespace video {

// Factory function implementation
IVideoDriver* createWebGPUDriver(const SIrrlichtCreationParameters& params,
                                io::IFileSystem* io, CIrrDeviceSDL* device) {

    os::Printer::log("Creating WebGPU driver...", ELL_INFORMATION);

    CWebGPUDriver* driver = new CWebGPUDriver(params, io, device);

    if (!driver->initializeWebGPU()) {
        os::Printer::log("Failed to initialize WebGPU", ELL_ERROR);
        driver->drop();
        return nullptr;
    }

    os::Printer::log("WebGPU driver created successfully", ELL_INFORMATION);
    return driver;
}

} // namespace video
} // namespace irr

#endif // _IRR_COMPILE_WITH_WEBGPU_