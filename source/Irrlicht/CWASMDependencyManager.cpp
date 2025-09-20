/*
 * Dependency Manager for Irrlicht.wasm Dynamic Library Loading
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license
 *
 * Manages dynamic loading of SIDE_MODULE dependencies (zlib, libpng, etc.)
 */

#include "CWASMDependencyManager.h"
#include <dlfcn.h>
#include <cstring>
#include <cstdio>

namespace irr {
namespace wasm {

class DependencyManager {
private:
    struct DependencyInfo {
        void* handle = nullptr;
        bool loaded = false;
        const char* name;
        const char* wasmPath;
    };

    DependencyInfo dependencies[4] = {
        {nullptr, false, "zlib", "zlib-side.wasm"},
        {nullptr, false, "libpng", "libpng-side.wasm"},
        {nullptr, false, "libjpeg", "libjpeg-turbo-side.wasm"},
        {nullptr, false, "bzip2", "bzip2-side.wasm"}
    };

    // Function pointers for dynamically loaded functions
    struct {
        // zlib functions
        int (*compress)(unsigned char* dest, unsigned long* destLen,
                       const unsigned char* source, unsigned long sourceLen);
        int (*uncompress)(unsigned char* dest, unsigned long* destLen,
                         const unsigned char* source, unsigned long sourceLen);

        // libpng functions
        void* (*png_create_read_struct)(const char* user_png_ver, void* error_ptr,
                                       void* error_fn, void* warn_fn);
        void* (*png_create_write_struct)(const char* user_png_ver, void* error_ptr,
                                        void* error_fn, void* warn_fn);
        void (*png_destroy_read_struct)(void** png_ptr_ptr, void** info_ptr_ptr, void** end_info_ptr_ptr);
        void (*png_destroy_write_struct)(void** png_ptr_ptr, void** info_ptr_ptr);

        // libjpeg functions
        void (*jpeg_std_error)(void* err);
        void (*jpeg_create_decompress)(void* cinfo);
        void (*jpeg_create_compress)(void* cinfo);
        int (*jpeg_read_header)(void* cinfo, int require_image);

        // bzip2 functions
        int (*BZ2_bzCompress)(void* strm, int action);
        int (*BZ2_bzDecompress)(void* strm);
    } funcs;

    bool useEmbedded = false;

public:
    static DependencyManager& getInstance() {
        static DependencyManager instance;
        return instance;
    }

    bool initializeDependencies() {
#ifdef IRRLICHT_USE_DYNAMIC_DEPS
        return loadDynamicDependencies();
#else
        return initializeEmbeddedDependencies();
#endif
    }

private:
    bool loadDynamicDependencies() {
        printf("🔗 Loading dynamic SIDE_MODULE dependencies...\n");

        for (int i = 0; i < 4; ++i) {
            dependencies[i].handle = dlopen(dependencies[i].wasmPath, RTLD_NOW);
            if (!dependencies[i].handle) {
                printf("⚠️  Failed to load %s: %s\n", dependencies[i].name, dlerror());
                // Don't fail completely - some dependencies might be optional
                continue;
            }
            dependencies[i].loaded = true;
            printf("✓ Loaded %s successfully\n", dependencies[i].name);
        }

        return bindDynamicFunctions();
    }

    bool bindDynamicFunctions() {
        // Bind zlib functions
        if (dependencies[0].loaded) {
            funcs.compress = (int(*)(unsigned char*, unsigned long*, const unsigned char*, unsigned long))
                dlsym(dependencies[0].handle, "compress");
            funcs.uncompress = (int(*)(unsigned char*, unsigned long*, const unsigned char*, unsigned long))
                dlsym(dependencies[0].handle, "uncompress");
        }

        // Bind libpng functions
        if (dependencies[1].loaded) {
            funcs.png_create_read_struct = (void*(*)(const char*, void*, void*, void*))
                dlsym(dependencies[1].handle, "png_create_read_struct");
            funcs.png_create_write_struct = (void*(*)(const char*, void*, void*, void*))
                dlsym(dependencies[1].handle, "png_create_write_struct");
            funcs.png_destroy_read_struct = (void(*)(void**, void**, void**))
                dlsym(dependencies[1].handle, "png_destroy_read_struct");
            funcs.png_destroy_write_struct = (void(*)(void**, void**))
                dlsym(dependencies[1].handle, "png_destroy_write_struct");
        }

        // Bind libjpeg functions
        if (dependencies[2].loaded) {
            funcs.jpeg_std_error = (void(*)(void*))dlsym(dependencies[2].handle, "jpeg_std_error");
            funcs.jpeg_create_decompress = (void(*)(void*))dlsym(dependencies[2].handle, "jpeg_create_decompress");
            funcs.jpeg_create_compress = (void(*)(void*))dlsym(dependencies[2].handle, "jpeg_create_compress");
            funcs.jpeg_read_header = (int(*)(void*, int))dlsym(dependencies[2].handle, "jpeg_read_header");
        }

        // Bind bzip2 functions
        if (dependencies[3].loaded) {
            funcs.BZ2_bzCompress = (int(*)(void*, int))dlsym(dependencies[3].handle, "BZ2_bzCompress");
            funcs.BZ2_bzDecompress = (int(*)(void*))dlsym(dependencies[3].handle, "BZ2_bzDecompress");
        }

        printf("✓ Dynamic function binding completed\n");
        return true;
    }

    bool initializeEmbeddedDependencies() {
        printf("📦 Using embedded static dependencies\n");
        useEmbedded = true;

        // Static linking - functions are available directly
        // No dynamic loading needed
        return true;
    }

public:
    // Wrapper functions that delegate to dynamic or embedded implementations
    int compressData(unsigned char* dest, unsigned long* destLen,
                    const unsigned char* source, unsigned long sourceLen) {
        if (useEmbedded) {
            // Use embedded zlib
            extern int compress(unsigned char*, unsigned long*, const unsigned char*, unsigned long);
            return compress(dest, destLen, source, sourceLen);
        } else if (funcs.compress) {
            return funcs.compress(dest, destLen, source, sourceLen);
        }
        return -1; // Error
    }

    int uncompressData(unsigned char* dest, unsigned long* destLen,
                      const unsigned char* source, unsigned long sourceLen) {
        if (useEmbedded) {
            extern int uncompress(unsigned char*, unsigned long*, const unsigned char*, unsigned long);
            return uncompress(dest, destLen, source, sourceLen);
        } else if (funcs.uncompress) {
            return funcs.uncompress(dest, destLen, source, sourceLen);
        }
        return -1;
    }

    void* createPNGReadStruct(const char* version) {
        if (useEmbedded) {
            extern void* png_create_read_struct(const char*, void*, void*, void*);
            return png_create_read_struct(version, nullptr, nullptr, nullptr);
        } else if (funcs.png_create_read_struct) {
            return funcs.png_create_read_struct(version, nullptr, nullptr, nullptr);
        }
        return nullptr;
    }

    // Additional wrapper functions for all needed library functions...
    bool isDependencyLoaded(const char* name) {
        for (int i = 0; i < 4; ++i) {
            if (strcmp(dependencies[i].name, name) == 0) {
                return dependencies[i].loaded || useEmbedded;
            }
        }
        return false;
    }

    void cleanup() {
        for (int i = 0; i < 4; ++i) {
            if (dependencies[i].handle) {
                dlclose(dependencies[i].handle);
                dependencies[i].handle = nullptr;
                dependencies[i].loaded = false;
            }
        }
    }
};

// Global access function
DependencyManager& getDependencyManager() {
    return DependencyManager::getInstance();
}

} // namespace wasm
} // namespace irr

// C API for external access
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    int irrlicht_init_dependencies() {
        return irr::wasm::getDependencyManager().initializeDependencies() ? 1 : 0;
    }

    EMSCRIPTEN_KEEPALIVE
    int irrlicht_dependency_loaded(const char* name) {
        return irr::wasm::getDependencyManager().isDependencyLoaded(name) ? 1 : 0;
    }

    EMSCRIPTEN_KEEPALIVE
    void irrlicht_cleanup_dependencies() {
        irr::wasm::getDependencyManager().cleanup();
    }
}