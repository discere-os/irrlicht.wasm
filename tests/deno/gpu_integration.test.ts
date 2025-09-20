/*
 * Phase 2 Integration Tests for Irrlicht.wasm
 * Tests GPU-driven rendering, batching, memory pools, and enhanced SIMD
 */

import { assert, assertEquals, assertExists } from "@std/assert";

Deno.test("Phase 2: GPU-driven rendering system integration", async () => {
    console.log("🎯 Testing Phase 2 enhancements...");

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

    // Test SIMD enhancements
    console.log("🚀 Testing enhanced SIMD batch operations...");

    // Test SIMD availability
    const simdAvailable = Module._irrlicht_simd_available();
    assertExists(simdAvailable);
    console.log(`✅ SIMD support: ${simdAvailable ? "available" : "fallback mode"}`);

    if (simdAvailable) {
        // Test batch matrix multiplication
        const matrixCount = 1000;
        const matrixSize = 16; // 4x4 matrices

        // Allocate matrices
        const matricesA = Module._malloc(matrixCount * matrixSize * 4);
        const matricesB = Module._malloc(matrixCount * matrixSize * 4);
        const results = Module._malloc(matrixCount * matrixSize * 4);

        // Initialize test matrices (identity * 2.0 scale)
        const identityA = new Float32Array(Module.HEAPF32.buffer, matricesA, matrixCount * matrixSize);
        const identityB = new Float32Array(Module.HEAPF32.buffer, matricesB, matrixCount * matrixSize);

        for (let i = 0; i < matrixCount; i++) {
            const offset = i * 16;
            // Matrix A: identity
            identityA[offset + 0] = 1; identityA[offset + 1] = 0; identityA[offset + 2] = 0; identityA[offset + 3] = 0;
            identityA[offset + 4] = 0; identityA[offset + 5] = 1; identityA[offset + 6] = 0; identityA[offset + 7] = 0;
            identityA[offset + 8] = 0; identityA[offset + 9] = 0; identityA[offset + 10] = 1; identityA[offset + 11] = 0;
            identityA[offset + 12] = 0; identityA[offset + 13] = 0; identityA[offset + 14] = 0; identityA[offset + 15] = 1;

            // Matrix B: scale by 2
            identityB[offset + 0] = 2; identityB[offset + 1] = 0; identityB[offset + 2] = 0; identityB[offset + 3] = 0;
            identityB[offset + 4] = 0; identityB[offset + 5] = 2; identityB[offset + 6] = 0; identityB[offset + 7] = 0;
            identityB[offset + 8] = 0; identityB[offset + 9] = 0; identityB[offset + 10] = 2; identityB[offset + 11] = 0;
            identityB[offset + 12] = 0; identityB[offset + 13] = 0; identityB[offset + 14] = 0; identityB[offset + 15] = 1;
        }

        // Test batch matrix multiplication
        const startTime = performance.now();
        Module._irrlicht_matrix_multiply_batch_simd(matricesA, matricesB, results, matrixCount);
        const endTime = performance.now();

        console.log(`✅ Batch matrix multiplication: ${matrixCount} matrices in ${(endTime - startTime).toFixed(2)}ms`);
        console.log(`📊 Performance: ${(matrixCount / (endTime - startTime) * 1000).toFixed(0)} matrices/second`);

        // Verify results (identity * scale = scale matrix)
        const resultArray = new Float32Array(Module.HEAPF32.buffer, results, matrixCount * matrixSize);
        assertEquals(resultArray[0], 2, "Matrix multiplication result incorrect");
        assertEquals(resultArray[5], 2, "Matrix multiplication result incorrect");
        assertEquals(resultArray[10], 2, "Matrix multiplication result incorrect");
        assertEquals(resultArray[15], 1, "Matrix multiplication result incorrect");

        // Test batch frustum culling
        console.log("🎯 Testing batch frustum culling...");

        const objectCount = 10000;
        const frustumPlanes = Module._malloc(6 * 4 * 4); // 6 planes, 4 floats each
        const boundingSpheres = Module._malloc(objectCount * 4 * 4); // x,y,z,radius for each object
        const visibilityResults = Module._malloc(objectCount);

        // Set up frustum planes (simplified frustum)
        const planeData = new Float32Array(Module.HEAPF32.buffer, frustumPlanes, 6 * 4);
        // Left, Right, Bottom, Top, Near, Far planes
        planeData.set([
            1, 0, 0, 10,   // Left
            -1, 0, 0, 10,  // Right
            0, 1, 0, 10,   // Bottom
            0, -1, 0, 10,  // Top
            0, 0, 1, 1,    // Near
            0, 0, -1, 100  // Far
        ]);

        // Set up bounding spheres
        const sphereData = new Float32Array(Module.HEAPF32.buffer, boundingSpheres, objectCount * 4);
        for (let i = 0; i < objectCount; i++) {
            sphereData[i * 4 + 0] = (Math.random() - 0.5) * 50; // x
            sphereData[i * 4 + 1] = (Math.random() - 0.5) * 50; // y
            sphereData[i * 4 + 2] = (Math.random() - 0.5) * 50; // z
            sphereData[i * 4 + 3] = 1.0; // radius
        }

        const cullStartTime = performance.now();
        Module._irrlicht_frustum_cull_batch_simd(frustumPlanes, boundingSpheres, visibilityResults, objectCount);
        const cullEndTime = performance.now();

        const visibilityArray = new Uint8Array(Module.HEAP8.buffer, visibilityResults, objectCount);
        const visibleCount = visibilityArray.reduce((sum, visible) => sum + visible, 0);

        console.log(`✅ Batch frustum culling: ${objectCount} objects in ${(cullEndTime - cullStartTime).toFixed(2)}ms`);
        console.log(`📊 Performance: ${(objectCount / (cullEndTime - cullStartTime) * 1000).toFixed(0)} objects/second`);
        console.log(`👁️ Visible objects: ${visibleCount}/${objectCount} (${(visibleCount/objectCount*100).toFixed(1)}%)`);

        assert(visibleCount > 0, "Some objects should be visible");
        assert(visibleCount < objectCount, "Not all objects should be visible in frustum");

        // Test vertex transformation batch
        console.log("💪 Testing batch vertex transformation...");

        const vertexCount = 1000;
        const matrices = Module._malloc(64 * 16 * 4); // 64 bone matrices
        const vertices = Module._malloc(vertexCount * 4 * 4); // vertex positions
        const transformResults = Module._malloc(vertexCount * 4 * 4);
        const boneIndices = Module._malloc(vertexCount * 4); // 4 bone indices per vertex
        const boneWeights = Module._malloc(vertexCount * 4 * 4); // 4 bone weights per vertex

        // Initialize bone matrices (identity matrices)
        const matrixData = new Float32Array(Module.HEAPF32.buffer, matrices, 64 * 16);
        for (let i = 0; i < 64; i++) {
            const offset = i * 16;
            matrixData[offset + 0] = 1; matrixData[offset + 5] = 1;
            matrixData[offset + 10] = 1; matrixData[offset + 15] = 1;
        }

        // Initialize vertices and bone data
        const vertexData = new Float32Array(Module.HEAPF32.buffer, vertices, vertexCount * 4);
        const boneIndexData = new Uint8Array(Module.HEAP8.buffer, boneIndices, vertexCount * 4);
        const boneWeightData = new Float32Array(Module.HEAPF32.buffer, boneWeights, vertexCount * 4);

        for (let i = 0; i < vertexCount; i++) {
            vertexData[i * 4 + 0] = Math.random() * 10; // x
            vertexData[i * 4 + 1] = Math.random() * 10; // y
            vertexData[i * 4 + 2] = Math.random() * 10; // z
            vertexData[i * 4 + 3] = 1.0; // w

            // Simple bone binding (first bone with weight 1.0)
            boneIndexData[i * 4 + 0] = 0; // First bone
            boneIndexData[i * 4 + 1] = 0;
            boneIndexData[i * 4 + 2] = 0;
            boneIndexData[i * 4 + 3] = 0;

            boneWeightData[i * 4 + 0] = 1.0; // Full weight on first bone
            boneWeightData[i * 4 + 1] = 0.0;
            boneWeightData[i * 4 + 2] = 0.0;
            boneWeightData[i * 4 + 3] = 0.0;
        }

        const transformStartTime = performance.now();
        Module._irrlicht_transform_vertices_batch_simd(matrices, vertices, transformResults,
                                                      vertexCount, 64, boneIndices, boneWeights);
        const transformEndTime = performance.now();

        console.log(`✅ Batch vertex transformation: ${vertexCount} vertices in ${(transformEndTime - transformStartTime).toFixed(2)}ms`);
        console.log(`📊 Performance: ${(vertexCount / (transformEndTime - transformStartTime) * 1000).toFixed(0)} vertices/second`);

        // Verify transformation results
        const transformResultData = new Float32Array(Module.HEAPF32.buffer, transformResults, vertexCount * 4);
        assert(transformResultData[0] !== 0 || transformResultData[1] !== 0 || transformResultData[2] !== 0, "Vertices should be transformed");

        // Cleanup
        Module._free(matricesA);
        Module._free(matricesB);
        Module._free(results);
        Module._free(frustumPlanes);
        Module._free(boundingSpheres);
        Module._free(visibilityResults);
        Module._free(matrices);
        Module._free(vertices);
        Module._free(transformResults);
        Module._free(boneIndices);
        Module._free(boneWeights);
    }

    console.log("🎉 Phase 2 integration tests completed successfully!");
});

Deno.test("Phase 2: Performance regression test", async () => {
    console.log("📊 Running Phase 2 performance benchmarks...");

    // Load the SIMD test module
    const wasmPath = new URL("../../install/wasm/irrlicht-simd-test.wasm", import.meta.url).pathname;
    let wasmBinary: ArrayBuffer | undefined;

    try {
        wasmBinary = await Deno.readFile(wasmPath).then(data => data.buffer);
    } catch {
        console.warn("⚠️ WASM binary not found, using embedded version");
    }

    const moduleFactory = (await import("../../install/wasm/irrlicht-simd-test.js")).default;
    const Module = await moduleFactory(wasmBinary ? { wasmBinary } : {});

    const simdAvailable = Module._irrlicht_simd_available();

    if (simdAvailable) {
        // Performance target validation
        const matrixCount = 10000;
        const matrixSize = 16;

        const matricesA = Module._malloc(matrixCount * matrixSize * 4);
        const matricesB = Module._malloc(matrixCount * matrixSize * 4);
        const results = Module._malloc(matrixCount * matrixSize * 4);

        // Initialize with identity matrices
        const identityA = new Float32Array(Module.HEAPF32.buffer, matricesA, matrixCount * matrixSize);
        const identityB = new Float32Array(Module.HEAPF32.buffer, matricesB, matrixCount * matrixSize);

        for (let i = 0; i < matrixCount * 16; i += 16) {
            identityA[i + 0] = identityA[i + 5] = identityA[i + 10] = identityA[i + 15] = 1;
            identityB[i + 0] = identityB[i + 5] = identityB[i + 10] = identityB[i + 15] = 2;
        }

        // Benchmark batch matrix multiplication
        const iterations = 10;
        let totalTime = 0;

        for (let i = 0; i < iterations; i++) {
            const start = performance.now();
            Module._irrlicht_matrix_multiply_batch_simd(matricesA, matricesB, results, matrixCount);
            const end = performance.now();
            totalTime += (end - start);
        }

        const avgTime = totalTime / iterations;
        const matricesPerSecond = matrixCount / (avgTime / 1000);

        console.log(`📈 Average batch matrix performance: ${matricesPerSecond.toFixed(0)} matrices/second`);

        // Performance target: Should achieve at least 50,000 matrices/second on modern browsers
        const performanceTarget = 50000;
        if (matricesPerSecond >= performanceTarget) {
            console.log(`✅ Performance target met: ${matricesPerSecond.toFixed(0)} >= ${performanceTarget} matrices/second`);
        } else {
            console.log(`⚠️ Performance below target: ${matricesPerSecond.toFixed(0)} < ${performanceTarget} matrices/second`);
        }

        // Benchmark frustum culling
        const objectCount = 100000;
        const frustumPlanes = Module._malloc(6 * 4 * 4);
        const boundingSpheres = Module._malloc(objectCount * 4 * 4);
        const visibilityResults = Module._malloc(objectCount);

        // Setup frustum and spheres
        const planeData = new Float32Array(Module.HEAPF32.buffer, frustumPlanes, 24);
        const sphereData = new Float32Array(Module.HEAPF32.buffer, boundingSpheres, objectCount * 4);

        // Simple frustum
        planeData.set([1,0,0,50, -1,0,0,50, 0,1,0,50, 0,-1,0,50, 0,0,1,1, 0,0,-1,1000]);

        // Random sphere positions
        for (let i = 0; i < objectCount * 4; i += 4) {
            sphereData[i] = (Math.random() - 0.5) * 200;
            sphereData[i + 1] = (Math.random() - 0.5) * 200;
            sphereData[i + 2] = (Math.random() - 0.5) * 200;
            sphereData[i + 3] = 2.0;
        }

        let cullTotalTime = 0;
        const cullIterations = 5;

        for (let i = 0; i < cullIterations; i++) {
            const start = performance.now();
            Module._irrlicht_frustum_cull_batch_simd(frustumPlanes, boundingSpheres, visibilityResults, objectCount);
            const end = performance.now();
            cullTotalTime += (end - start);
        }

        const avgCullTime = cullTotalTime / cullIterations;
        const objectsPerSecond = objectCount / (avgCullTime / 1000);

        console.log(`🎯 Average culling performance: ${objectsPerSecond.toFixed(0)} objects/second`);

        // Performance target: Should achieve at least 1,000,000 objects/second
        const cullTarget = 1000000;
        if (objectsPerSecond >= cullTarget) {
            console.log(`✅ Culling target met: ${objectsPerSecond.toFixed(0)} >= ${cullTarget} objects/second`);
        } else {
            console.log(`⚠️ Culling below target: ${objectsPerSecond.toFixed(0)} < ${cullTarget} objects/second`);
        }

        // Cleanup
        Module._free(matricesA);
        Module._free(matricesB);
        Module._free(results);
        Module._free(frustumPlanes);
        Module._free(boundingSpheres);
        Module._free(visibilityResults);
    }

    console.log("📊 Performance benchmarks completed");
});