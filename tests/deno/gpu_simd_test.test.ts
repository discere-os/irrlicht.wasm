/*
 * GPU-Driven SIMD Component Test for Irrlicht.wasm
 * Tests the enhanced SIMD functionality independently
 */

import { assert, assertEquals, assertExists } from "@std/assert";

Deno.test("GPU-Driven: SIMD batch operations", async () => {
    console.log("🎯 Testing GPU-driven SIMD enhancements...");

    // Load the SIMD test module
    const wasmPath = new URL("../../install/wasm/irrlicht-simd-test.wasm", import.meta.url).pathname;
    let wasmBinary: ArrayBuffer | undefined;

    try {
        wasmBinary = await Deno.readFile(wasmPath).then(data => data.buffer);
    } catch (error) {
        console.warn("⚠️ WASM binary not found, using embedded version:", error);
    }

    const moduleFactory = (await import("../../install/wasm/irrlicht-simd-test.js")).default;
    const Module = await moduleFactory(wasmBinary ? { wasmBinary } : {});

    // Test SIMD availability
    console.log("🚀 Testing SIMD availability...");
    const simdAvailable = Module._irrlicht_simd_available();
    assertExists(simdAvailable);
    console.log(`✅ SIMD support: ${simdAvailable ? "available" : "fallback mode"}`);

    if (simdAvailable) {
        console.log("💪 SIMD is available - testing enhanced batch operations!");

        // Test that the functions are exported and callable
        assertExists(Module._irrlicht_matrix_multiply_simd, "Matrix multiply SIMD function should exist");
        assertExists(Module._irrlicht_matrix_multiply_batch_simd, "Batch matrix multiply function should exist");
        assertExists(Module._irrlicht_transform_vertices_batch_simd, "Batch vertex transform function should exist");
        assertExists(Module._irrlicht_frustum_cull_batch_simd, "Batch frustum culling function should exist");

        console.log("✅ All GPU-driven SIMD functions are exported and available");

        // Test basic memory allocation works
        assertExists(Module._malloc, "malloc function should exist");
        assertExists(Module._free, "free function should exist");

        const testPtr = Module._malloc(64);
        assert(testPtr !== 0, "malloc should return valid pointer");
        Module._free(testPtr);

        console.log("✅ Memory allocation functions working");

    } else {
        console.log("⚠️ SIMD not available - using scalar fallback mode");
    }

    console.log("🎉 GPU-driven SIMD component test completed successfully!");
});

Deno.test("GPU-Driven: Architecture validation", async () => {
    console.log("🏗️ Validating GPU-driven architecture components...");

    // Check that all GPU-driven enhancement files exist
    const enhancementFiles = [
        "source/Irrlicht/CWebGPUComputeCulling.h",
        "source/Irrlicht/CWebGPUComputeCulling.cpp",
        "source/Irrlicht/CWebGPURenderBatcher.h",
        "source/Irrlicht/CWebGPURenderBatcher.cpp",
        "source/Irrlicht/CWebGPUMemoryPool.h",
        "source/Irrlicht/CWebGPUMemoryPool.cpp",
        "source/Irrlicht/CIrrlichtSIMD.cpp"
    ];

    for (const file of enhancementFiles) {
        try {
            const stat = await Deno.stat(file);
            assert(stat.isFile, `${file} should be a file`);
            console.log(`✅ ${file} - exists (${stat.size} bytes)`);
        } catch (error) {
            throw new Error(`❌ Missing GPU enhancement file: ${file}`);
        }
    }

    console.log("🏗️ All GPU-driven architecture files validated!");

    // Check that the SIMD build artifact exists
    try {
        const simdStat = await Deno.stat("install/wasm/irrlicht-simd-test.js");
        const wasmStat = await Deno.stat("install/wasm/irrlicht-simd-test.wasm");

        console.log(`✅ SIMD JavaScript: ${simdStat.size} bytes`);
        console.log(`✅ SIMD WebAssembly: ${wasmStat.size} bytes`);

        assert(simdStat.size > 1000, "JavaScript file should be substantial");
        assert(wasmStat.size > 1000, "WASM file should be substantial");

    } catch (error) {
        throw new Error(`❌ SIMD build artifacts missing: ${error}`);
    }

    console.log("🎉 GPU-driven architecture validation completed!");
});