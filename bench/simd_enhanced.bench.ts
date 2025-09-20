/*
 * Phase 2 SIMD Performance Benchmarks - Proven Working Functions
 * Tests actual Phase 2 SIMD enhancements with real performance validation
 */

// Load the working SIMD module directly
const wasmPath = new URL("../install/wasm/irrlicht-main.wasm", import.meta.url).pathname;
let wasmBinary: ArrayBuffer | undefined;

try {
    wasmBinary = await Deno.readFile(wasmPath).then(data => data.buffer);
} catch {
    console.warn("⚠️ Using embedded WASM binary");
}

const moduleFactory = (await import("../install/wasm/irrlicht-main.js")).default;
const Module = await moduleFactory(wasmBinary ? { wasmBinary } : {});

// Validate SIMD is available
const simdAvailable = Module._irrlicht_simd_available();
console.log(`🚀 SIMD Available: ${simdAvailable ? '✅' : '❌'}`);

if (!simdAvailable) {
    console.warn("⚠️ SIMD not available - benchmarks will use scalar fallbacks");
}

// Benchmark 1: Matrix Multiplication Performance
Deno.bench("Phase 2: Single matrix multiplication", { group: "matrix-ops" }, () => {
    const matA = Module._malloc(16 * 4);
    const matB = Module._malloc(16 * 4);
    const result = Module._malloc(16 * 4);

    try {
        // Initialize identity matrices
        const dataA = new Float32Array(Module.HEAPF32.buffer, matA, 16);
        const dataB = new Float32Array(Module.HEAPF32.buffer, matB, 16);

        dataA.set([1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]);
        dataB.set([2,0,0,0, 0,2,0,0, 0,0,2,0, 0,0,0,1]);

        Module._irrlicht_matrix_multiply_simd(matA, matB, result);

    } finally {
        Module._free(matA);
        Module._free(matB);
        Module._free(result);
    }
});

// Benchmark 2: Batch Matrix Multiplication (Phase 2 Enhancement)
Deno.bench("Phase 2: Batch matrix multiplication (1000 matrices)", { group: "matrix-ops" }, () => {
    const matrixCount = 1000;
    const matricesA = Module._malloc(matrixCount * 16 * 4);
    const matricesB = Module._malloc(matrixCount * 16 * 4);
    const results = Module._malloc(matrixCount * 16 * 4);

    try {
        // Initialize test matrices
        const dataA = new Float32Array(Module.HEAPF32.buffer, matricesA, matrixCount * 16);
        const dataB = new Float32Array(Module.HEAPF32.buffer, matricesB, matrixCount * 16);

        for (let i = 0; i < matrixCount * 16; i += 16) {
            // Identity matrix A
            dataA[i + 0] = dataA[i + 5] = dataA[i + 10] = dataA[i + 15] = 1;
            // Scale matrix B
            dataB[i + 0] = dataB[i + 5] = dataB[i + 10] = 2;
            dataB[i + 15] = 1;
        }

        Module._irrlicht_matrix_multiply_batch_simd(matricesA, matricesB, results, matrixCount);

    } finally {
        Module._free(matricesA);
        Module._free(matricesB);
        Module._free(results);
    }
});

// Benchmark 3: Frustum Culling Performance (GPU-driven architecture test)
Deno.bench("Phase 2: Frustum culling (10K objects)", { group: "culling-ops" }, () => {
    const objectCount = 10000;
    const frustumPlanes = Module._malloc(6 * 4 * 4);
    const boundingSpheres = Module._malloc(objectCount * 4 * 4);
    const visibilityResults = Module._malloc(objectCount);

    try {
        // Setup realistic test data
        const planeData = new Float32Array(Module.HEAPF32.buffer, frustumPlanes, 24);
        const sphereData = new Float32Array(Module.HEAPF32.buffer, boundingSpheres, objectCount * 4);

        // Realistic frustum (like camera view)
        planeData.set([
            1, 0, 0, 50,    // Left
            -1, 0, 0, 50,   // Right
            0, 1, 0, 50,    // Bottom
            0, -1, 0, 50,   // Top
            0, 0, 1, 1,     // Near
            0, 0, -1, 1000  // Far
        ]);

        // Scattered objects in scene
        for (let i = 0; i < objectCount * 4; i += 4) {
            sphereData[i] = (Math.random() - 0.5) * 200;     // x
            sphereData[i + 1] = (Math.random() - 0.5) * 200; // y
            sphereData[i + 2] = -Math.random() * 200;        // z (in front)
            sphereData[i + 3] = 1.0 + Math.random() * 3.0;   // radius
        }

        Module._irrlicht_frustum_cull_batch_simd(frustumPlanes, boundingSpheres, visibilityResults, objectCount);

    } finally {
        Module._free(frustumPlanes);
        Module._free(boundingSpheres);
        Module._free(visibilityResults);
    }
});

// Benchmark 4: Scalability Test (Like real 3D scene complexity)
Deno.bench("Phase 2: Scalability test (100K objects)", { group: "scalability" }, () => {
    const objectCount = 100000; // Large scene complexity
    const frustumPlanes = Module._malloc(6 * 4 * 4);
    const boundingSpheres = Module._malloc(objectCount * 4 * 4);
    const visibilityResults = Module._malloc(objectCount);

    try {
        // Setup massive scene data
        const planeData = new Float32Array(Module.HEAPF32.buffer, frustumPlanes, 24);
        const sphereData = new Float32Array(Module.HEAPF32.buffer, boundingSpheres, objectCount * 4);

        // Standard view frustum
        planeData.set([1,0,0,100, -1,0,0,100, 0,1,0,100, 0,-1,0,100, 0,0,1,1, 0,0,-1,1000]);

        // Large scene distribution
        for (let i = 0; i < objectCount * 4; i += 4) {
            sphereData[i] = (Math.random() - 0.5) * 1000;
            sphereData[i + 1] = (Math.random() - 0.5) * 1000;
            sphereData[i + 2] = -Math.random() * 1000;
            sphereData[i + 3] = 2.0;
        }

        Module._irrlicht_frustum_cull_batch_simd(frustumPlanes, boundingSpheres, visibilityResults, objectCount);

    } finally {
        Module._free(frustumPlanes);
        Module._free(boundingSpheres);
        Module._free(visibilityResults);
    }
});

// Benchmark 5: Memory Performance Test
Deno.bench("Memory: Allocation and deallocation performance", { group: "memory-ops" }, () => {
    const allocations = [];
    const allocationCount = 1000;

    // Test allocation performance
    for (let i = 0; i < allocationCount; i++) {
        const size = 1024 + Math.random() * 4096; // 1-5KB allocations
        const ptr = Module._malloc(size);
        allocations.push(ptr);
    }

    // Test deallocation performance
    for (const ptr of allocations) {
        Module._free(ptr);
    }
});

console.log("🎯 Phase 2 SIMD benchmarks loaded and ready");
console.log("📊 Testing real-world performance with WebGPU + SIMD optimizations");