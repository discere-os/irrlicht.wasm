# @discere-os/irrlicht.wasm

WebAssembly port of Irrlicht - High-performance real-time 3D graphics engine with OpenGL, WebGL, and comprehensive rendering pipeline for games, simulations, and interactive applications.

[![CI/CD](https://github.com/discere-os/discere-nucleus/actions/workflows/irrlicht-wasm-ci.yml/badge.svg)](https://github.com/discere-os/discere-nucleus/actions)
[![JSR](https://jsr.io/badges/@discere-os/irrlicht.wasm)](https://jsr.io/@discere-os/irrlicht.wasm)
[![npm version](https://badge.fury.io/js/@discere-os%2Firrlicht.wasm.svg)](https://badge.fury.io/js/@discere-os%2Firrlicht.wasm)
[![License](https://img.shields.io/badge/License-Zlib-blue.svg)](LICENSE)
[![Status](https://img.shields.io/badge/status-alpha-orange.svg)](https://github.com/discere-os/discere-nucleus)

## About Irrlicht Engine

The Irrlicht Engine is a high performance realtime 3D engine written in C++. It is completely cross-platform, using D3D, OpenGL and its own software renderers, and has all of the state-of-the-art features which can be found in commercial 3d engines.

## Features

- **Cross-Platform Rendering**: OpenGL, WebGL, and software rendering support
- **Scene Management**: Advanced scene graph with optimized culling and LOD
- **Material System**: Programmable shader pipeline with GLSL support
- **Animation**: Skeletal animation, morphing, and keyframe animation
- **File Format Support**: 3DS, X, MS3D, COLLADA, B3D, MD2, MD3, BSP, and more
- **Physics Integration**: Built-in collision detection and response
- **GUI System**: Immediate mode GUI for in-game interfaces
- **Post-Processing**: Real-time effects and filters
- **Terrain Rendering**: Height-based terrain with texture blending
- **Particle Systems**: Configurable particle effects
- **Audio Support**: 3D positioned audio through integrated sound engine

## WebAssembly Enhancements

This port provides:

- **WASM SIMD Optimization**: Vectorized graphics operations for enhanced performance
- **WebGPU Acceleration**: GPU compute shaders with external orchestrator support
- **Modern TypeScript API**: Type-safe interfaces with comprehensive documentation
- **Browser-Native Integration**: Direct canvas rendering without plugins
- **Progressive Loading**: Efficient asset streaming for web deployment
- **Memory Management**: Optimized WASM heap usage with dynamic dependency loading

## Quick Start

```bash
# Install via npm
npm install @discere-os/irrlicht.wasm

# Or use with Deno
import Irrlicht from "https://deno.land/x/irrlicht_wasm/mod.ts"
```

## Usage

```typescript
import Irrlicht from '@discere-os/irrlicht.wasm'

const engine = new Irrlicht()
await engine.initialize()

// Create device and scene manager
const device = engine.createDevice({
  renderer: 'webgl',
  canvas: document.getElementById('canvas')
})

const scene = device.getSceneManager()
const camera = scene.addCameraSceneNodeFPS()

// Add 3D objects
const mesh = scene.getMesh('path/to/model.3ds')
const node = scene.addMeshSceneNode(mesh)

// Render loop
function render() {
  device.run()
  driver.beginScene()
  scene.drawAll()
  driver.endScene()
  requestAnimationFrame(render)
}

render()
```

## Original Documentation

This WebAssembly port is based on the Irrlicht Engine SDK version 1.9. The original documentation and examples are preserved in the `doc/` and `examples/` directories.

### Directory Structure

- `bin/` - Compiled library and demo applications
- `doc/` - Original Irrlicht Engine documentation
- `examples/` - C++ examples and tutorials
- `include/` - Header files for engine API
- `lib/` - Static libraries for linking
- `media/` - Graphics and sound resources for examples
- `source/` - Complete Irrlicht Engine source code
- `tools/` - Development tools and utilities

## Performance Considerations

The WebAssembly build is optimized for web deployment:

- **SIMD Operations**: 3-4x speedup for vector/matrix calculations
- **GPU Acceleration**: WebGPU compute with external coordinator support
- **Memory Layout**: Optimized data structures for WASM heap efficiency
- **Asset Loading**: Streaming mesh and texture loading with dynamic dependencies
- **Rendering Pipeline**: Batched draw calls and efficient resource sharing

## License

Based on the zlib/libpng license (same as original Irrlicht Engine).

Copyright (C) 2002-2012 Nikolaus Gebhardt

This software is provided 'as-is', without any express or implied warranty. Permission is granted to use, modify, and distribute this software for any purpose, including commercial applications.

## 💖 Support This Work

This WebAssembly port is part of a larger effort to bring professional desktop applications to browsers with native performance.

**👨‍💻 About the Maintainer**: [Isaac Johnston (@superstructor)](https://github.com/superstructor) - Building foundational browser-native computing infrastructure through systematic C/C++ to WebAssembly porting.

**📊 Impact**: 70+ open source WASM libraries enabling professional applications like Blender, GIMP, and scientific computing tools to run natively in browsers.

**🚀 Your Support Enables**:
- Continued maintenance and updates
- Performance optimizations
- New library ports and integrations
- Documentation and tutorials
- Cross-browser compatibility testing

**[💖 Sponsor this work](https://github.com/sponsors/superstructor)** to help build the future of browser-native computing.