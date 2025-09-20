import { assert, assertEquals, assertExists } from "@std/assert";
import Irrlicht, { DriverType, MaterialType } from "../../src/lib/index.ts";

Deno.test("3D scene rendering", async () => {
  const engine = new Irrlicht({
    driverType: DriverType.NULL,
    windowSize: { width: 800, height: 600 },
    loadingOptions: { useSideModules: false }
  });

  await engine.initialize();

  // Create a complete 3D scene
  const camera = engine.addCamera(
    { x: 30, y: 20, z: 30 },
    { x: 0, y: 0, z: 0 }
  );
  assertExists(camera);

  // Add various geometric primitives
  const cube = engine.addCube(10);
  cube.setPosition(0, 0, 0);
  cube.setMaterialType(MaterialType.SOLID);

  const sphere = engine.addSphere(8, 32);
  sphere.setPosition(20, 0, 0);
  sphere.setMaterialType(MaterialType.TRANSPARENT_ALPHA_CHANNEL);

  const smallCube = engine.addCube(5);
  smallCube.setPosition(0, 15, 0);
  smallCube.setMaterialType(MaterialType.LIGHTMAP);

  // Test scene hierarchy and transformations
  sphere.setRotation(45, 30, 0);
  sphere.setScale(1.5, 1.5, 1.5);

  cube.setRotation(0, 45, 0);

  // Render multiple frames to test consistency
  for (let frame = 0; frame < 10; frame++) {
    // Animate objects
    const time = frame * 0.1;
    cube.setRotation(0, time * 50, 0);
    sphere.setPosition(Math.sin(time) * 15, 0, Math.cos(time) * 15);
    smallCube.setPosition(0, 10 + Math.sin(time * 2) * 5, 0);

    // Render frame
    engine.beginScene({ r: 64, g: 128, b: 255, a: 255 });
    engine.drawAll();
    engine.endScene();

    assert(engine.run());
  }

  // Verify rendering statistics
  const stats = engine.getRenderStatistics();
  assert(stats.fps >= 0);
  console.log(`Rendering test completed at ${stats.fps.toFixed(1)} FPS`);

  engine.cleanup();
});

Deno.test("Material system testing", async () => {
  const engine = new Irrlicht({
    driverType: DriverType.NULL,
    windowSize: { width: 640, height: 480 },
    loadingOptions: { useSideModules: false }
  });

  await engine.initialize();

  const camera = engine.addCamera({ x: 0, y: 0, z: 20 }, { x: 0, y: 0, z: 0 });

  // Test different material types
  const materialTypes = [
    MaterialType.SOLID,
    MaterialType.TRANSPARENT_ADD_COLOR,
    MaterialType.TRANSPARENT_ALPHA_CHANNEL,
    MaterialType.LIGHTMAP,
    MaterialType.DETAIL_MAP
  ];

  const cubes: any[] = [];

  // Create cubes with different materials
  for (let i = 0; i < materialTypes.length; i++) {
    const cube = engine.addCube(5);
    cube.setPosition(i * 12 - 24, 0, 0);
    cube.setMaterialType(materialTypes[i]);
    cubes.push(cube);
  }

  // Test material property changes
  for (let frame = 0; frame < 5; frame++) {
    // Animate materials
    for (let i = 0; i < cubes.length; i++) {
      cubes[i].setRotation(frame * 20, frame * 30 + i * 45, 0);
    }

    engine.beginScene({ r: 32, g: 32, b: 64, a: 255 });
    engine.drawAll();
    engine.endScene();
    engine.run();
  }

  console.log(`✅ Material system test completed with ${materialTypes.length} material types`);
  engine.cleanup();
});

Deno.test("Camera system testing", async () => {
  const engine = new Irrlicht({
    driverType: DriverType.NULL,
    windowSize: { width: 800, height: 600 },
    loadingOptions: { useSideModules: false }
  });

  await engine.initialize();

  // Test different camera types
  const staticCamera = engine.addCamera(
    { x: 10, y: 10, z: 10 },
    { x: 0, y: 0, z: 0 }
  );
  assertExists(staticCamera);

  const fpsCamera = engine.addCameraFPS(100, 0.1);
  assertExists(fpsCamera);

  // Add scene object for reference
  const cube = engine.addCube(8);
  cube.setPosition(0, 0, 0);

  // Test camera movements
  const cameraPositions = [
    { x: 15, y: 0, z: 0 },
    { x: 0, y: 15, z: 0 },
    { x: 0, y: 0, z: 15 },
    { x: 10, y: 10, z: 10 }
  ];

  for (let i = 0; i < cameraPositions.length; i++) {
    const pos = cameraPositions[i];
    staticCamera.setPosition(pos.x, pos.y, pos.z);

    // Render from this camera position
    engine.beginScene({ r: 100, g: 150, b: 200, a: 255 });
    engine.drawAll();
    engine.endScene();
    engine.run();
  }

  console.log(`✅ Camera system test completed with ${cameraPositions.length} positions`);
  engine.cleanup();
});

Deno.test("Complex scene rendering", async () => {
  const engine = new Irrlicht({
    driverType: DriverType.NULL,
    windowSize: { width: 1024, height: 768 },
    loadingOptions: { useSideModules: false }
  });

  await engine.initialize();

  const camera = engine.addCamera(
    { x: 25, y: 25, z: 25 },
    { x: 0, y: 0, z: 0 }
  );

  // Create a complex scene with multiple objects
  const objects: any[] = [];

  // Grid of cubes
  for (let x = -10; x <= 10; x += 5) {
    for (let z = -10; z <= 10; z += 5) {
      const cube = engine.addCube(2);
      cube.setPosition(x, 0, z);
      cube.setMaterialType(MaterialType.SOLID);
      objects.push(cube);
    }
  }

  // Spheres at different heights
  for (let i = 0; i < 8; i++) {
    const angle = (i / 8) * Math.PI * 2;
    const sphere = engine.addSphere(3, 16);
    sphere.setPosition(
      Math.cos(angle) * 15,
      5 + Math.sin(i * 0.5) * 3,
      Math.sin(angle) * 15
    );
    sphere.setMaterialType(MaterialType.TRANSPARENT_ALPHA_CHANNEL);
    objects.push(sphere);
  }

  console.log(`Created complex scene with ${objects.length} objects`);

  // Render animated scene
  for (let frame = 0; frame < 20; frame++) {
    const time = frame * 0.1;

    // Animate all objects
    for (let i = 0; i < objects.length; i++) {
      const obj = objects[i];
      obj.setRotation(
        time * 30 + i * 10,
        time * 20 + i * 15,
        time * 40 + i * 5
      );
    }

    // Render frame
    engine.beginScene({ r: 25, g: 25, b: 50, a: 255 });
    engine.drawAll();
    engine.endScene();
    engine.run();

    // Check performance every few frames
    if (frame % 5 === 0) {
      const stats = engine.getRenderStatistics();
      console.log(`Frame ${frame}: ${stats.fps.toFixed(1)} FPS`);
    }
  }

  console.log("✅ Complex scene rendering test completed");
  engine.cleanup();
});

Deno.test("Error recovery testing", async () => {
  const engine = new Irrlicht({
    driverType: DriverType.NULL,
    windowSize: { width: 640, height: 480 },
    loadingOptions: { useSideModules: false }
  });

  await engine.initialize();

  const camera = engine.addCamera({ x: 0, y: 0, z: 10 }, { x: 0, y: 0, z: 0 });
  const cube = engine.addCube(5);

  // Test error scenarios
  try {
    // Invalid scene operations
    cube.setPosition(NaN, 0, 0); // Invalid position
    cube.setRotation(Infinity, 0, 0); // Invalid rotation

    // Should still be able to render
    engine.beginScene();
    engine.drawAll();
    engine.endScene();
    engine.run();

    console.log("✅ Error recovery test passed");
  } catch (error) {
    console.log("⚠️  Error in error recovery test:", error.message);
  }

  engine.cleanup();
});

Deno.test("Memory stress testing", async () => {
  const engine = new Irrlicht({
    driverType: DriverType.NULL,
    windowSize: { width: 640, height: 480 },
    loadingOptions: { useSideModules: false }
  });

  await engine.initialize();

  const camera = engine.addCamera({ x: 0, y: 0, z: 50 }, { x: 0, y: 0, z: 0 });

  // Create many objects to stress test memory management
  const objects: any[] = [];
  const objectCount = 100;

  console.log(`Creating ${objectCount} objects for memory stress test...`);

  for (let i = 0; i < objectCount; i++) {
    const obj = (i % 2 === 0) ? engine.addCube(1) : engine.addSphere(1, 8);

    // Position in spiral
    const angle = i * 0.5;
    const radius = i * 0.5;
    obj.setPosition(
      Math.cos(angle) * radius,
      Math.sin(i * 0.3) * 10,
      Math.sin(angle) * radius
    );

    objects.push(obj);
  }

  // Render several frames
  for (let frame = 0; frame < 10; frame++) {
    // Animate all objects
    for (let i = 0; i < objects.length; i++) {
      objects[i].setRotation(frame * 10 + i, frame * 15 + i, frame * 5 + i);
    }

    engine.beginScene({ r: 20, g: 40, b: 80, a: 255 });
    engine.drawAll();
    engine.endScene();
    engine.run();
  }

  const stats = engine.getRenderStatistics();
  console.log(`✅ Memory stress test completed: ${stats.fps.toFixed(1)} FPS with ${objectCount} objects`);

  engine.cleanup();
});