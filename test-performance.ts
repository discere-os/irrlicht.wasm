import Irrlicht, {DriverType} from "./src/lib/index.ts";

const engine = new Irrlicht({
  driverType: DriverType.NULL,
  loadingOptions: {useSideModules: false}
});

await engine.initialize();
const metrics = await engine.benchmarkPerformance();

console.log("📈 Performance Metrics:");
console.log(`Matrix: ${metrics.matrixMultiplication.speedup.toFixed(2)}x speedup`);
console.log(`Vertex: ${metrics.vertexTransformation.speedup.toFixed(2)}x speedup`);
console.log(`Throughput: ${metrics.vertexTransformation.verticesPerSecond.toLocaleString()} vertices/sec`);

engine.cleanup();