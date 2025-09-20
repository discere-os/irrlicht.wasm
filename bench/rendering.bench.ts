import Irrlicht, { DriverType, checkSIMDSupport } from "../src/lib/index.ts";

let engine: Irrlicht;

// Setup before benchmarks
await (async () => {
  engine = new Irrlicht({
    driverType: DriverType.NULL,
    windowSize: { width: 800, height: 600 },
    loadingOptions: { useSideModules: false },
    simdOptimizations: true
  });

  await engine.initialize();
  console.log(`🚀 Benchmarking with SIMD: ${checkSIMDSupport() ? 'enabled' : 'disabled'}`);
})();

// Scene creation benchmark
Deno.bench("Scene creation with 100 objects", () => {
  const objects: any[] = [];

  for (let i = 0; i < 100; i++) {
    const obj = (i % 2 === 0) ? engine.addCube(1) : engine.addSphere(1, 8);
    obj.setPosition(
      (i % 10 - 5) * 2,
      Math.floor(i / 10) * 2,
      0
    );
    objects.push(obj);
  }

  // Cleanup objects (in real implementation)
  // for (const obj of objects) { obj.remove(); }
});

// Rendering throughput benchmark
Deno.bench("Rendering throughput (50 objects)", () => {
  // Create scene
  const camera = engine.addCamera({ x: 0, y: 0, z: 25 }, { x: 0, y: 0, z: 0 });
  const objects: any[] = [];

  for (let i = 0; i < 50; i++) {
    const obj = engine.addCube(1);
    obj.setPosition(
      (i % 7 - 3) * 3,
      Math.floor(i / 7) * 3 - 10,
      0
    );
    objects.push(obj);
  }

  // Render frame
  engine.beginScene({ r: 50, g: 100, b: 150, a: 255 });
  engine.drawAll();
  engine.endScene();
  engine.run();
});

// Matrix operations benchmark
Deno.bench("Matrix multiplication (1000 operations)", async () => {
  const metrics = await engine.benchmarkPerformance();
  // The benchmark is run internally
  console.log(`Matrix performance: ${metrics.matrixMultiplication.speedup.toFixed(2)}x speedup`);
});

// Vertex transformation benchmark
Deno.bench("Vertex transformation (10000 vertices)", async () => {
  const metrics = await engine.benchmarkPerformance();
  console.log(`Vertex performance: ${metrics.vertexTransformation.verticesPerSecond.toLocaleString()} vertices/sec`);
});

// Material switching benchmark
Deno.bench("Material switching (20 materials)", () => {
  const objects: any[] = [];

  // Create objects with different materials
  for (let i = 0; i < 20; i++) {
    const obj = engine.addCube(2);
    obj.setPosition(i * 2 - 19, 0, 0);

    // Cycle through material types
    const materialTypes = [0, 1, 2, 3, 12, 13, 14, 15]; // Various material types
    obj.setMaterialType(materialTypes[i % materialTypes.length]);
    objects.push(obj);
  }

  engine.beginScene();
  engine.drawAll();
  engine.endScene();
  engine.run();
});

// Camera movement benchmark
Deno.bench("Camera transformations (100 positions)", () => {
  const camera = engine.addCamera({ x: 0, y: 0, z: 10 }, { x: 0, y: 0, z: 0 });
  const cube = engine.addCube(5);

  for (let i = 0; i < 100; i++) {
    const angle = (i / 100) * Math.PI * 2;
    camera.setPosition(
      Math.cos(angle) * 15,
      Math.sin(angle * 0.5) * 5,
      Math.sin(angle) * 15
    );

    // Don't render every position to focus on camera performance
    if (i % 10 === 0) {
      engine.beginScene();
      engine.drawAll();
      engine.endScene();
    }
  }
});

// Memory allocation benchmark
Deno.bench("Object creation/destruction cycle", () => {
  const objects: any[] = [];

  // Create objects
  for (let i = 0; i < 50; i++) {
    const obj = engine.addSphere(1, 8);
    obj.setPosition(i, 0, 0);
    objects.push(obj);
  }

  // Use objects
  for (const obj of objects) {
    obj.setRotation(45, 30, 0);
  }

  // Note: In real implementation, would destroy objects here
  // This benchmark tests the creation overhead
});

// Complex scene benchmark
Deno.bench("Complex scene with lighting and materials", () => {
  const camera = engine.addCamera({ x: 20, y: 20, z: 20 }, { x: 0, y: 0, z: 0 });

  // Create complex scene
  const objects: any[] = [];

  // Grid of objects with different materials
  for (let x = -5; x <= 5; x += 2) {
    for (let z = -5; z <= 5; z += 2) {
      const obj = ((x + z) % 2 === 0) ? engine.addCube(1) : engine.addSphere(1, 12);
      obj.setPosition(x, 0, z);

      // Vary materials
      if ((x + z) % 4 === 0) {
        obj.setMaterialType(0); // Solid
      } else if ((x + z) % 4 === 1) {
        obj.setMaterialType(12); // Transparent add
      } else {
        obj.setMaterialType(13); // Transparent alpha
      }

      objects.push(obj);
    }
  }

  // Render complex scene
  engine.beginScene({ r: 25, g: 50, b: 100, a: 255 });
  engine.drawAll();
  engine.endScene();
  engine.run();
});

// SIMD vs Scalar performance comparison
Deno.bench("SIMD optimization effectiveness", async () => {
  if (!checkSIMDSupport()) {
    console.log("⏭️  SIMD not supported, skipping comparison");
    return;
  }

  const metrics = await engine.benchmarkPerformance();

  // Log performance improvements
  console.log(`\n🚀 SIMD Performance Results:`);
  console.log(`Matrix multiplication: ${metrics.matrixMultiplication.speedup.toFixed(2)}x faster`);
  console.log(`Vertex transformation: ${metrics.vertexTransformation.speedup.toFixed(2)}x faster`);
  console.log(`Vertices per second: ${metrics.vertexTransformation.verticesPerSecond.toLocaleString()}`);

  // Verify reasonable speedups
  assert(metrics.matrixMultiplication.speedup >= 1.0);
  assert(metrics.vertexTransformation.speedup >= 1.0);
});

// Cleanup after benchmarks
globalThis.addEventListener("unload", () => {
  engine?.cleanup();
  console.log("🧹 Benchmark cleanup completed");
});