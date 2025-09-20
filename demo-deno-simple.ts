#!/usr/bin/env -S deno run --allow-read --allow-write

/**
 * Simple Irrlicht.wasm Demo - Minimal example
 */

import Irrlicht, { DriverType } from "./src/lib/index.ts";

console.log("🎮 Simple Irrlicht.wasm Demo");
console.log("============================");

const engine = new Irrlicht({
  driverType: DriverType.NULL, // Headless for simplicity
  windowSize: { width: 640, height: 480 }
});

await engine.initialize();
console.log("✅ Engine initialized");

// Create simple scene
const camera = engine.addCamera(
  { x: 0, y: 0, z: 10 },
  { x: 0, y: 0, z: 0 }
);

const cube = engine.addCube(5);
console.log("📦 Added cube to scene");

// Render one frame
engine.beginScene({ r: 100, g: 150, b: 200, a: 255 });
engine.drawAll();
engine.endScene();
engine.run();

console.log("🎬 Rendered one frame");

// Performance test
const metrics = await engine.benchmarkPerformance();
console.log(`⚡ Matrix speedup: ${metrics.matrixMultiplication.speedup.toFixed(2)}x`);

engine.cleanup();
console.log("✅ Simple demo completed");