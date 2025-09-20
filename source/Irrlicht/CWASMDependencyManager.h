/*
 * Dependency Manager Header for Irrlicht.wasm
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license
 */

#ifndef IRR_C_WASM_DEPENDENCY_MANAGER_H_INCLUDED
#define IRR_C_WASM_DEPENDENCY_MANAGER_H_INCLUDED

namespace irr {
namespace wasm {

class DependencyManager;

// Global access to dependency manager
DependencyManager& getDependencyManager();

} // namespace wasm
} // namespace irr

// C API declarations
extern "C" {
    int irrlicht_init_dependencies();
    int irrlicht_dependency_loaded(const char* name);
    void irrlicht_cleanup_dependencies();
}

#endif // IRR_C_WASM_DEPENDENCY_MANAGER_H_INCLUDED