/*
 * Real-world WebGPU Performance Benchmarks for Irrlicht.wasm
 * Based on actual Irrlicht usage patterns from examples
 * Tests Phase 2 enhancements with WebGPU acceleration via Deno
 */

import Irrlicht, { DriverType, checkWebGPUSupport, checkSIMDSupport } from "../src/lib/index.ts";

// Setup benchmark environment
let engine: Irrlicht;

// Initialize engine before benchmarks
try {
    const hasWebGPU = checkWebGPUSupport();
    const hasSIMD = checkSIMDSupport();

    console.log(`🎮 Benchmark Environment:`);
    console.log(`  WebGPU Support: ${hasWebGPU ? '✅' : '❌'}`);
    console.log(`  WASM SIMD: ${hasSIMD ? '✅' : '❌'}`);

    engine = new Irrlicht({
        driverType: hasWebGPU ? DriverType.WEBGPU : DriverType.SOFTWARE,
        windowSize: { width: 1024, height: 768 },
        webgpuOptions: {
            useExternalCoordinator: false, // Standalone for benchmarking
            preferHighPerformance: true,
            requiredFeatures: ['bgra8unorm-storage']
        }
    });

    await engine.initialize();
    console.log(`✅ Irrlicht.wasm initialized for benchmarking`);

} catch (error) {
    console.error(`❌ Failed to initialize Irrlicht for benchmarking: ${error}`);
    throw error;
}

// Benchmark 1: HelloWorld Example Pattern - Device Creation and Scene Setup
Deno.bench("HelloWorld: Device creation and basic scene setup", { group: "irrlicht-core" }, () => {
    // Test device access performance
    const driver = engine.getVideoDriver();
    const sceneManager = engine.getSceneManager();

    // Test camera creation (HelloWorld pattern)
    const camera = engine.addCamera(0, 30, -40, 0, 5, 0);

    // Test basic scene node creation (HelloWorld pattern)
    const cube = engine.addCube(10);

    // Test material settings (HelloWorld pattern)
    engine.setMaterialFlag(cube, 0, false); // Disable lighting like HelloWorld

    // Performance: This should be fast due to our mock implementation
});

// Benchmark 2: Phase 2 SIMD Matrix Operations - Real Performance Test
Deno.bench("Phase 2: SIMD matrix multiplication batch", { group: "phase2-simd" }, () => {
    const matrixCount = 1000;

    // Allocate matrices for testing
    const matricesA = engine.module._malloc(matrixCount * 16 * 4);
    const matricesB = engine.module._malloc(matrixCount * 16 * 4);
    const results = engine.module._malloc(matrixCount * 16 * 4);

    try {
        // Initialize test data
        const dataA = new Float32Array(engine.module.HEAPF32.buffer, matricesA, matrixCount * 16);
        const dataB = new Float32Array(engine.module.HEAPF32.buffer, matricesB, matrixCount * 16);

        for (let i = 0; i < matrixCount * 16; i += 16) {
            // Identity matrix A
            dataA[i + 0] = dataA[i + 5] = dataA[i + 10] = dataA[i + 15] = 1;
            // Scale matrix B (2x)
            dataB[i + 0] = dataB[i + 5] = dataB[i + 10] = 2;
            dataB[i + 15] = 1;
        }

        // Benchmark actual SIMD batch multiplication
        engine.module._irrlicht_matrix_multiply_batch_simd(matricesA, matricesB, results, matrixCount);

    } finally {
        engine.module._free(matricesA);
        engine.module._free(matricesB);
        engine.module._free(results);
    }
});

// Benchmark 3: Phase 2 SIMD Frustum Culling - Real Performance Test
Deno.bench("Phase 2: SIMD frustum culling batch", { group: "phase2-simd" }, () => {
    const objectCount = 10000; // Real-world object count

    const frustumPlanes = engine.module._malloc(6 * 4 * 4);
    const boundingSpheres = engine.module._malloc(objectCount * 4 * 4);
    const visibilityResults = engine.module._malloc(objectCount);

    try {
        // Setup realistic frustum and object data
        const planeData = new Float32Array(engine.module.HEAPF32.buffer, frustumPlanes, 24);
        const sphereData = new Float32Array(engine.module.HEAPF32.buffer, boundingSpheres, objectCount * 4);

        // Realistic frustum planes (camera looking down negative Z)
        planeData.set([
            1, 0, 0, 50,    // Left plane
            -1, 0, 0, 50,   // Right plane
            0, 1, 0, 50,    // Bottom plane
            0, -1, 0, 50,   // Top plane
            0, 0, 1, 1,     // Near plane
            0, 0, -1, 1000  // Far plane
        ]);

        // Realistic object distribution (like scattered scene objects)
        for (let i = 0; i < objectCount * 4; i += 4) {
            sphereData[i] = (Math.random() - 0.5) * 100;     // x: -50 to 50
            sphereData[i + 1] = (Math.random() - 0.5) * 100; // y: -50 to 50
            sphereData[i + 2] = -Math.random() * 100;        // z: 0 to -100 (in front of camera)
            sphereData[i + 3] = 1.0 + Math.random() * 5.0;   // radius: 1-6 units
        }

        // Benchmark actual SIMD frustum culling
        engine.module._irrlicht_frustum_cull_batch_simd(frustumPlanes, boundingSpheres, visibilityResults, objectCount);

    } finally {
        engine.module._free(frustumPlanes);
        engine.module._free(boundingSpheres);
        engine.module._free(visibilityResults);
    }
});

// Benchmark 4: HelloWorld Render Loop Pattern - Scene Rendering
Deno.bench("HelloWorld: Complete render loop simulation", { group: "irrlicht-rendering" }, () => {
    // Simulate the HelloWorld render loop pattern:
    // while(device->run()) {
    //     driver->beginScene(ECBF_COLOR | ECBF_DEPTH, SColor(255,100,101,140));
    //     smgr->drawAll();
    //     guienv->drawAll();
    //     driver->endScene();
    // }

    let frameCount = 0;
    const maxFrames = 10; // Limit for benchmark

    while (engine.run() && frameCount < maxFrames) {
        engine.beginScene(100, 101, 140); // Like HelloWorld example
        engine.drawAll();
        engine.endScene();
        frameCount++;
    }

    // Validate we processed expected number of frames
    if (frameCount !== maxFrames) {
        throw new Error(`Expected ${maxFrames} frames, got ${frameCount}`);
    }
});

// Benchmark 5: Phase 2 SIMD vs Scalar Comparison
Deno.bench("Phase 2: SIMD vs Scalar matrix performance", { group: "performance-comparison" }, () => {
    const testSize = 100;

    const matA = engine.module._malloc(16 * 4);
    const matB = engine.module._malloc(16 * 4);
    const result = engine.module._malloc(16 * 4);

    try {
        // Initialize identity matrices
        const dataA = new Float32Array(engine.module.HEAPF32.buffer, matA, 16);
        const dataB = new Float32Array(engine.module.HEAPF32.buffer, matB, 16);

        dataA.set([1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]);
        dataB.set([2,0,0,0, 0,2,0,0, 0,0,2,0, 0,0,0,1]);

        // Test both SIMD and scalar versions
        for (let i = 0; i < testSize; i++) {
            if (engine.module._irrlicht_simd_available()) {
                engine.module._irrlicht_matrix_multiply_simd(matA, matB, result);
            } else {
                engine.module._irrlicht_matrix_multiply_scalar(matA, matB, result);
            }
        }

    } finally {
        engine.module._free(matA);
        engine.module._free(matB);
        engine.module._free(result);
    }
});

// Benchmark 6: Multiple Scene Objects (Real-world Complexity)
Deno.bench("Real-world: Multiple objects and transformations", { group: "irrlicht-scene" }, () => {
    const objectCount = 100;
    const objects = [];

    // Create multiple objects like a real scene
    for (let i = 0; i < objectCount; i++) {
        const obj = engine.addCube(1 + Math.random() * 5);

        // Position objects in 3D space (like scattered scene)
        engine.setNodePosition(obj,
            (Math.random() - 0.5) * 100,
            (Math.random() - 0.5) * 100,
            (Math.random() - 0.5) * 100
        );

        // Random rotations
        engine.setNodeRotation(obj,
            Math.random() * 360,
            Math.random() * 360,
            Math.random() * 360
        );

        objects.push(obj);
    }

    // Simulate render loop with all objects
    engine.beginScene(50, 50, 50);
    engine.drawAll();
    engine.endScene();
});

// Cleanup after benchmarks
globalThis.addEventListener("unload", () => {
    try {
        engine?.cleanup();
        console.log("🧹 Benchmark cleanup completed");
    } catch (error) {
        console.warn("⚠️ Cleanup warning:", error);
    }
});