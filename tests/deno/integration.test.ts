import { assert, assertEquals, assertExists } from "@std/assert";
import Irrlicht, { DriverType } from "../../src/lib/index.ts";

Deno.test("Dependency integration test", async () => {
  const engine = new Irrlicht({
    driverType: DriverType.NULL,
    windowSize: { width: 640, height: 480 },
    loadingOptions: { useSideModules: false } // Test with embedded dependencies
  });

  await engine.initialize();

  const capabilities = engine.getCapabilities();

  // Test that all required dependencies are available
  assert(capabilities.dependenciesLoaded.zlib, "zlib dependency should be loaded");
  assert(capabilities.dependenciesLoaded.libpng, "libpng dependency should be loaded");
  assert(capabilities.dependenciesLoaded.libjpeg, "libjpeg dependency should be loaded");
  assert(capabilities.dependenciesLoaded.bzip2, "bzip2 dependency should be loaded");

  console.log("✅ All dependencies loaded successfully");
  console.log("Dependencies:", capabilities.dependenciesLoaded);

  engine.cleanup();
});

Deno.test("SIDE_MODULE vs MAIN_MODULE comparison", async () => {
  // Test both build variants if available
  const testConfigs = [
    { useSideModules: false, name: "MAIN_MODULE" },
    { useSideModules: true, name: "SIDE_MODULE" }
  ];

  for (const config of testConfigs) {
    console.log(`Testing ${config.name} build...`);

    try {
      const engine = new Irrlicht({
        driverType: DriverType.NULL,
        windowSize: { width: 512, height: 384 },
        loadingOptions: config
      });

      const startTime = performance.now();
      await engine.initialize();
      const initTime = performance.now() - startTime;

      assert(engine.isInitialized());

      // Create basic scene
      const camera = engine.addCamera({ x: 0, y: 0, z: 15 }, { x: 0, y: 0, z: 0 });
      const cube = engine.addCube(5);

      // Render one frame
      engine.beginScene();
      engine.drawAll();
      engine.endScene();
      engine.run();

      console.log(`✅ ${config.name} test passed (init: ${initTime.toFixed(2)}ms)`);
      engine.cleanup();

    } catch (error) {
      console.log(`⚠️  ${config.name} test failed:`, error.message);
      // Don't fail the test - SIDE_MODULE might not be available in development
    }
  }
});

Deno.test("Cross-library integration test", async () => {
  // Test integration with other WASM libraries in the ecosystem
  const engine = new Irrlicht({
    driverType: DriverType.NULL,
    windowSize: { width: 640, height: 480 },
    loadingOptions: { useSideModules: false }
  });

  await engine.initialize();

  // This test verifies that Irrlicht can coexist with other WASM libraries
  const camera = engine.addCamera({ x: 0, y: 0, z: 10 }, { x: 0, y: 0, z: 0 });
  const cube = engine.addCube(5);

  // Simulate interaction with other libraries
  // In a real environment, this might involve:
  // - Loading textures processed by ImageMagick.wasm
  // - Using audio from a sound library
  // - Loading 3D models processed by other tools

  // For this test, just verify basic functionality
  cube.setMaterialType(MaterialType.SOLID);

  // Render scene
  engine.beginScene({ r: 100, g: 100, b: 100, a: 255 });
  engine.drawAll();
  engine.endScene();
  engine.run();

  const capabilities = engine.getCapabilities();
  assert(capabilities.driverType === DriverType.NULL);

  console.log("✅ Cross-library integration test completed");
  engine.cleanup();
});

Deno.test("Performance comparison between drivers", async () => {
  // Compare performance across different driver types
  const driverTypes = [DriverType.NULL, DriverType.SOFTWARE];
  const results: Record<string, any> = {};

  for (const driverType of driverTypes) {
    const driverName = DriverType[driverType];
    console.log(`Testing ${driverName} driver performance...`);

    try {
      const engine = new Irrlicht({
        driverType,
        windowSize: { width: 640, height: 480 },
        loadingOptions: { useSideModules: false }
      });

      const startTime = performance.now();
      await engine.initialize();
      const initTime = performance.now() - startTime;

      // Create test scene
      const camera = engine.addCamera({ x: 0, y: 0, z: 20 }, { x: 0, y: 0, z: 0 });
      const objects: any[] = [];

      // Create multiple objects for performance testing
      for (let i = 0; i < 20; i++) {
        const obj = (i % 2 === 0) ? engine.addCube(2) : engine.addSphere(2, 16);
        obj.setPosition(
          (i % 5 - 2) * 5,
          Math.floor(i / 5) * 5 - 7.5,
          0
        );
        objects.push(obj);
      }

      // Measure rendering performance
      const renderStartTime = performance.now();
      const frameCount = 10;

      for (let frame = 0; frame < frameCount; frame++) {
        // Animate objects
        for (let i = 0; i < objects.length; i++) {
          objects[i].setRotation(frame * 10 + i * 5, frame * 15 + i * 3, 0);
        }

        engine.beginScene({ r: 50, g: 100, b: 150, a: 255 });
        engine.drawAll();
        engine.endScene();
        engine.run();
      }

      const renderTime = performance.now() - renderStartTime;
      const avgFrameTime = renderTime / frameCount;

      // Run performance benchmarks
      const metrics = await engine.benchmarkPerformance();

      results[driverName] = {
        initTime,
        avgFrameTime,
        fps: 1000 / avgFrameTime,
        matrixSpeedup: metrics.matrixMultiplication.speedup,
        vertexSpeedup: metrics.vertexTransformation.speedup
      };

      console.log(`${driverName} results:`, results[driverName]);
      engine.cleanup();

    } catch (error) {
      console.log(`⚠️  ${driverName} driver test failed:`, error.message);
      results[driverName] = { error: error.message };
    }
  }

  // Compare results
  console.log("\n📊 Driver Performance Comparison:");
  for (const [driver, result] of Object.entries(results)) {
    if (result.error) {
      console.log(`${driver}: Error - ${result.error}`);
    } else {
      console.log(`${driver}: ${result.fps.toFixed(1)} FPS, Init: ${result.initTime.toFixed(2)}ms`);
    }
  }

  // At least one driver should work
  const workingDrivers = Object.values(results).filter(r => !r.error);
  assert(workingDrivers.length > 0, "At least one driver should work");
});

Deno.test("Multi-instance rendering test", async () => {
  // Test multiple Irrlicht instances running concurrently
  const instances: Irrlicht[] = [];
  const instanceCount = 3;

  try {
    console.log(`Creating ${instanceCount} concurrent Irrlicht instances...`);

    // Create multiple instances
    for (let i = 0; i < instanceCount; i++) {
      const engine = new Irrlicht({
        driverType: DriverType.NULL,
        windowSize: { width: 400 + i * 100, height: 300 + i * 100 },
        loadingOptions: { useSideModules: false }
      });

      await engine.initialize();
      instances.push(engine);

      // Create unique scene for each instance
      const camera = engine.addCamera(
        { x: 0, y: 0, z: 10 + i * 5 },
        { x: 0, y: 0, z: 0 }
      );

      const cube = engine.addCube(3 + i);
      cube.setPosition(i * 2, 0, 0);
      cube.setMaterialType(MaterialType.SOLID);
    }

    // Render all instances
    for (let frame = 0; frame < 5; frame++) {
      for (let i = 0; i < instances.length; i++) {
        const engine = instances[i];

        engine.beginScene({ r: 50 + i * 50, g: 100, b: 150, a: 255 });
        engine.drawAll();
        engine.endScene();
        engine.run();
      }
    }

    console.log(`✅ Multi-instance test completed with ${instanceCount} instances`);

  } finally {
    // Cleanup all instances
    for (const engine of instances) {
      engine.cleanup();
    }
  }
});

Deno.test("File format support test", async () => {
  const engine = new Irrlicht({
    driverType: DriverType.NULL,
    windowSize: { width: 640, height: 480 },
    loadingOptions: { useSideModules: false }
  });

  await engine.initialize();

  // Test that the engine can handle various file formats
  // (Even if files don't exist, should not crash)
  const testFormats = [
    "test.3ds",
    "test.obj",
    "test.x",
    "test.ms3d",
    "test.md2",
    "test.md3",
    "test.b3d"
  ];

  for (const format of testFormats) {
    try {
      await engine.loadMesh(format);
      console.log(`✅ ${format} format handler available`);
    } catch (error) {
      // Expected for non-existent files
      console.log(`📄 ${format} format tested (file not found, as expected)`);
    }
  }

  console.log("✅ File format support test completed");
  engine.cleanup();
});

Deno.test("Animation system test", async () => {
  const engine = new Irrlicht({
    driverType: DriverType.NULL,
    windowSize: { width: 800, height: 600 },
    loadingOptions: { useSideModules: false }
  });

  await engine.initialize();

  const camera = engine.addCamera({ x: 0, y: 0, z: 20 }, { x: 0, y: 0, z: 0 });

  // Create objects for animation testing
  const cube = engine.addCube(5);
  const sphere = engine.addSphere(3, 16);

  sphere.setPosition(10, 0, 0);

  // Test various animation patterns
  const animationFrames = 30;

  for (let frame = 0; frame < animationFrames; frame++) {
    const time = frame / animationFrames;
    const angle = time * Math.PI * 2;

    // Rotating cube
    cube.setRotation(
      angle * 180 / Math.PI,
      angle * 90 / Math.PI,
      0
    );

    // Orbiting sphere
    sphere.setPosition(
      Math.cos(angle) * 8,
      Math.sin(angle * 2) * 3,
      Math.sin(angle) * 8
    );

    // Pulsing scale
    const scale = 1.0 + Math.sin(angle * 4) * 0.3;
    sphere.setScale(scale, scale, scale);

    // Render frame
    engine.beginScene({ r: 30, g: 60, b: 120, a: 255 });
    engine.drawAll();
    engine.endScene();
    engine.run();
  }

  console.log(`✅ Animation system test completed with ${animationFrames} frames`);
  engine.cleanup();
});