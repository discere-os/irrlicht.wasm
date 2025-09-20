import { assert, assertEquals, assertExists } from "@std/assert";
import Irrlicht, { DriverType, checkWebGPUSupport, checkSIMDSupport } from "../../src/lib/index.ts";

Deno.test("Irrlicht module initialization", async () => {
  const engine = new Irrlicht({
    driverType: DriverType.NULL, // Use null driver for headless testing
    windowSize: { width: 640, height: 480 },
    loadingOptions: { useSideModules: false } // Use MAIN_MODULE for testing
  });

  await engine.initialize();
  assertExists(engine);
  assert(engine.isInitialized());

  engine.cleanup();
});

Deno.test("Device capabilities detection", async () => {
  const engine = new Irrlicht({
    driverType: DriverType.WEBGPU,  // Test our Phase 2 WebGPU driver
    windowSize: { width: 800, height: 600 },
    loadingOptions: { useSideModules: false }
  });

  await engine.initialize();

  const capabilities = engine.getCapabilities();
  assertExists(capabilities);
  // Driver type should be WEBGPU for Phase 2 implementation
  assertEquals(capabilities.driverType, DriverType.WEBGPU);
  assertExists(capabilities.driverName);
  assert(typeof capabilities.simdSupported === 'boolean');

  engine.cleanup();
});

Deno.test("Scene creation and basic operations", async () => {
  const engine = new Irrlicht({
    driverType: DriverType.NULL,
    windowSize: { width: 640, height: 480 },
    loadingOptions: { useSideModules: false }
  });

  await engine.initialize();

  // Create basic scene objects
  const camera = engine.addCamera(
    { x: 0, y: 0, z: 10 },
    { x: 0, y: 0, z: 0 }
  );
  assertExists(camera);

  const cube = engine.addCube(5);
  assertExists(cube);

  const sphere = engine.addSphere(3, 16);
  assertExists(sphere);

  // Test scene manipulation
  cube.setPosition(5, 0, 0);
  cube.setRotation(45, 0, 0);
  cube.setScale(2, 2, 2);

  sphere.setPosition(-5, 0, 0);

  // Test render loop (single frame)
  engine.beginScene({ r: 100, g: 100, b: 200, a: 255 });
  engine.drawAll();
  engine.endScene();

  assert(engine.run());

  engine.cleanup();
});

Deno.test("WebGPU support detection", () => {
  const webgpuSupported = checkWebGPUSupport();
  assert(typeof webgpuSupported === 'boolean');
  console.log(`WebGPU support: ${webgpuSupported ? '✅' : '❌'}`);
});

Deno.test("SIMD support detection", () => {
  const simdSupported = checkSIMDSupport();
  assert(typeof simdSupported === 'boolean');
  console.log(`WASM SIMD support: ${simdSupported ? '✅' : '❌'}`);
});

Deno.test("Driver type enumeration", () => {
  assertEquals(DriverType.NULL, 0);
  assertEquals(DriverType.SOFTWARE, 1);
  assertEquals(DriverType.OPENGL, 5);
  assertEquals(DriverType.WEBGPU, 6);
});

Deno.test("Render statistics collection", async () => {
  const engine = new Irrlicht({
    driverType: DriverType.NULL,
    windowSize: { width: 800, height: 600 },
    loadingOptions: { useSideModules: false }
  });

  await engine.initialize();

  // Add some objects to render
  engine.addCube(10);
  engine.addSphere(5, 32);

  // Render a few frames
  for (let i = 0; i < 3; i++) {
    engine.beginScene();
    engine.drawAll();
    engine.endScene();
    engine.run();
  }

  const stats = engine.getRenderStatistics();
  assertExists(stats);
  assert(typeof stats.fps === 'number');
  assert(typeof stats.frameTime === 'number');
  assert(stats.fps >= 0);

  engine.cleanup();
});

Deno.test("Error handling", async () => {
  // Test initialization with invalid parameters
  const engine = new Irrlicht({
    driverType: DriverType.WEBGPU, // Might not be available in test environment
    windowSize: { width: 0, height: 0 }, // Invalid size
    loadingOptions: { useSideModules: false }
  });

  try {
    await engine.initialize();
    // If it succeeds, that's fine too
    engine.cleanup();
  } catch (error) {
    // Error is expected with WebGPU in headless environment
    assert(error instanceof Error);
    assertExists(error.message);
  }
});

Deno.test("Memory management", async () => {
  // Test multiple initialization cycles
  for (let i = 0; i < 3; i++) {
    const engine = new Irrlicht({
      driverType: DriverType.NULL,
      windowSize: { width: 320, height: 240 },
      loadingOptions: { useSideModules: false }
    });

    await engine.initialize();
    assert(engine.isInitialized());

    // Create and destroy objects
    const cube = engine.addCube(5);
    const camera = engine.addCamera({ x: 0, y: 0, z: 5 }, { x: 0, y: 0, z: 0 });

    // Render one frame
    engine.beginScene();
    engine.drawAll();
    engine.endScene();

    engine.cleanup();
    assert(!engine.isInitialized());
  }
});

Deno.test("Concurrent engine instances", async () => {
  // Test multiple concurrent Irrlicht instances
  const engines: Irrlicht[] = [];

  try {
    for (let i = 0; i < 2; i++) {
      const engine = new Irrlicht({
        driverType: DriverType.NULL,
        windowSize: { width: 400 + i * 100, height: 300 + i * 100 },
        loadingOptions: { useSideModules: false }
      });

      await engine.initialize();
      engines.push(engine);
    }

    // Test that all engines work
    for (const engine of engines) {
      assert(engine.isInitialized());
      engine.addCube(5);
      engine.beginScene();
      engine.drawAll();
      engine.endScene();
    }

  } finally {
    // Cleanup all engines
    for (const engine of engines) {
      engine.cleanup();
    }
  }
});

Deno.test("Performance benchmarking", async () => {
  const engine = new Irrlicht({
    driverType: DriverType.NULL,
    windowSize: { width: 640, height: 480 },
    loadingOptions: { useSideModules: false }
  });

  await engine.initialize();

  const metrics = await engine.benchmarkPerformance();
  assertExists(metrics);
  assertExists(metrics.matrixMultiplication);
  assertExists(metrics.vertexTransformation);

  // Verify performance metrics are reasonable
  assert(metrics.matrixMultiplication.simdTime >= 0);
  assert(metrics.vertexTransformation.verticesPerSecond > 0);

  if (checkSIMDSupport()) {
    assert(metrics.matrixMultiplication.speedup > 1.0);
    assert(metrics.vertexTransformation.speedup > 1.0);
    console.log(`Matrix multiplication speedup: ${metrics.matrixMultiplication.speedup.toFixed(2)}x`);
    console.log(`Vertex transformation speedup: ${metrics.vertexTransformation.speedup.toFixed(2)}x`);
  }

  engine.cleanup();
});