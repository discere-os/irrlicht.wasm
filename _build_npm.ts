#!/usr/bin/env -S deno run --allow-all

/**
 * NPM Package Builder for Irrlicht.wasm
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license
 */

import { build, emptyDir } from "https://deno.land/x/dnt@0.38.1/mod.ts";

console.log("📦 Building NPM package for @discere-os/irrlicht.wasm");

await emptyDir("./npm");

await build({
  entryPoints: ["./src/lib/index.ts"],
  outDir: "./npm",
  shims: {
    // Provide Node.js compatibility shims
    deno: true,
    webApi: true,
    crypto: true,
    blob: true,
  },
  package: {
    name: "@discere-os/irrlicht.wasm",
    version: "1.9.0",
    description: "WebAssembly port of Irrlicht 3D Engine with WebGPU acceleration and SIMD optimization",
    keywords: [
      "irrlicht",
      "3d",
      "engine",
      "webassembly",
      "wasm",
      "webgpu",
      "simd",
      "graphics",
      "rendering",
      "game-engine"
    ],
    license: "Zlib",
    author: {
      name: "Isaac Johnston",
      email: "isaac@discere.school",
      url: "https://github.com/superstructor"
    },
    repository: {
      type: "git",
      url: "git+https://github.com/discere-os/discere-nucleus.git",
      directory: "client/emscripten/irrlicht.wasm"
    },
    bugs: {
      url: "https://github.com/discere-os/discere-nucleus/issues"
    },
    homepage: "https://github.com/discere-os/discere-nucleus/tree/main/client/emscripten/irrlicht.wasm",
    funding: {
      type: "github",
      url: "https://github.com/sponsors/superstructor"
    },
    main: "./esm/lib/index.js",
    module: "./esm/lib/index.js",
    types: "./esm/lib/index.d.ts",
    exports: {
      ".": {
        import: "./esm/lib/index.js",
        require: "./script/lib/index.js",
        types: "./esm/lib/index.d.ts"
      },
      "./types": {
        import: "./esm/lib/types.js",
        require: "./script/lib/types.js",
        types: "./esm/lib/types.d.ts"
      }
    },
    files: [
      "esm/",
      "script/",
      "dist/",
      "README.md",
      "LICENSE"
    ],
    engines: {
      node: ">=18.0.0"
    },
    sideEffects: false
  },
  postBuild() {
    // Copy WASM artifacts to NPM package
    console.log("📦 Copying WASM artifacts...");

    try {
      Deno.copyFileSync("install/wasm/irrlicht-main.js", "npm/dist/irrlicht-main.js");
      Deno.copyFileSync("install/wasm/irrlicht-main.wasm", "npm/dist/irrlicht-main.wasm");
      console.log("✅ WASM artifacts copied");
    } catch (error) {
      console.warn("⚠️  Could not copy WASM artifacts:", error.message);
    }

    // Copy README and LICENSE
    try {
      Deno.copyFileSync("README.md", "npm/README.md");
      console.log("✅ README copied");
    } catch (error) {
      console.warn("⚠️  Could not copy README:", error.message);
    }

    // Create package.json scripts
    const packageJson = JSON.parse(Deno.readTextFileSync("npm/package.json"));
    packageJson.scripts = {
      "test": "echo 'Run tests with: deno test' && exit 0",
      "build": "echo 'Already built' && exit 0",
      "demo": "echo 'Run demo with: deno task demo' && exit 0"
    };

    packageJson.browser = {
      "./dist/irrlicht-main.js": "./dist/irrlicht-main.js"
    };

    Deno.writeTextFileSync("npm/package.json", JSON.stringify(packageJson, null, 2));
    console.log("✅ Package.json updated");

    console.log("\n📦 NPM package build completed!");
    console.log("To publish: cd npm && npm publish");
  },
});

console.log("✅ NPM build process completed");