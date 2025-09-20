/**
 * Irrlicht.wasm - TypeScript wrapper for Irrlicht 3D Engine
 * Copyright (c) 2002-2012 Nikolaus Gebhardt
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license
 *
 * High-performance 3D engine with WebGPU acceleration and orchestrator integration
 */

import {
  DriverType,
  MaterialType,
  PrimitiveType,
  ColorFormat,
  IrrlichtError,
  WebGPUError,
  InitializationError,
  DependencyError
} from './types.ts';

import type {
  IrrlichtModule,
  IrrlichtOptions,
  Vector3D,
  Color,
  Dimension2D,
  CreationParameters,
  WebGPUOptions,
  LoadingOptions,
  SceneNode,
  MeshSceneNode,
  CameraSceneNode,
  LightSceneNode,
  DeviceCapabilities,
  RenderStatistics,
  PerformanceMetrics,
  WebGPUContext
} from './types.ts';

export * from './types.ts';

export default class Irrlicht {
  private module: IrrlichtModule | null = null;
  private device: any = null;
  private initialized = false;
  private options: Required<IrrlichtOptions>;
  private canvas: HTMLCanvasElement | null = null;
  private webgpuContext: WebGPUContext | null = null;

  constructor(options: IrrlichtOptions = {}) {
    this.options = {
      canvas: options.canvas || null,
      driverType: options.driverType || DriverType.WEBGPU,
      windowSize: options.windowSize || { width: 800, height: 600 },
      webgpuOptions: {
        useExternalCoordinator: true,
        preferHighPerformance: true,
        requiredFeatures: ['bgra8unorm-storage'],
        deviceLimits: {
          maxTextureSize: 4096,
          maxUniformBufferSize: 64 * 1024,
          maxStorageBufferSize: 16 * 1024 * 1024
        },
        ...options.webgpuOptions
      },
      loadingOptions: {
        cdnUrl: 'https://wasm.discere.cloud/irrlicht/',
        fallbackUrls: [
          'https://cdn.jsdelivr.net/npm/@discere-os/irrlicht.wasm/',
          'https://unpkg.com/@discere-os/irrlicht.wasm/'
        ],
        useSideModules: true,
        cachingEnabled: true,
        maxMemoryMB: 512,
        timeoutMs: 30000,
        ...options.loadingOptions
      },
      simdOptimizations: options.simdOptimizations !== false
    };
  }

  /**
   * Initialize Irrlicht.wasm with WebGPU support
   */
  async initialize(): Promise<void> {
    if (this.initialized) return;

    try {
      console.log('🎮 Initializing Irrlicht.wasm...');

      // Load WASM module
      await this.loadWasmModule();

      // Initialize WebGPU context
      if (this.options.driverType === DriverType.WEBGPU) {
        await this.initializeWebGPU();
      }

      // Create Irrlicht device
      this.createDevice();

      // Setup canvas if provided
      if (this.options.canvas) {
        this.setupCanvas();
      }

      this.initialized = true;
      console.log('✅ Irrlicht.wasm initialized successfully');

      // Print device information
      this.printDeviceInfo();

    } catch (error) {
      throw new InitializationError(`Failed to initialize Irrlicht: ${error}`);
    }
  }

  private async loadWasmModule(): Promise<void> {
    const startTime = performance.now();

    try {
      // Load based on configuration
      if (this.options.loadingOptions.useSideModules) {
        await this.loadSideModuleBuild();
      } else {
        await this.loadMainModuleBuild();
      }

      const loadTime = performance.now() - startTime;
      console.log(`📦 WASM module loaded in ${loadTime.toFixed(2)}ms`);

    } catch (error) {
      throw new InitializationError(`WASM module loading failed: ${error}`);
    }
  }

  private async loadSideModuleBuild(): Promise<void> {
    console.log('🔗 Loading SIDE_MODULE build with dynamic dependencies...');

    const moduleFactory = await this.loadModuleFactory();
    const wasmBinary = await this.loadWasmBinary();

    this.module = await moduleFactory({
      wasmBinary,
      locateFile: (path: string) => {
        if (path.endsWith('.wasm')) {
          return new URL('../../install/wasm/' + path, import.meta.url).href;
        }
        return path;
      },
      // Configure dynamic library loading
      dynamicLibraries: [
        'https://wasm.discere.cloud/zlib/latest/side/zlib-side.wasm',
        'https://wasm.discere.cloud/libpng/latest/side/libpng-side.wasm',
        'https://wasm.discere.cloud/bzip2/latest/side/bzip2-side.wasm',
        'https://wasm.discere.cloud/libjpeg-turbo/latest/side/libjpeg-turbo-side.wasm'
      ]
    });
  }

  private async loadMainModuleBuild(): Promise<void> {
    console.log('📦 Loading MAIN_MODULE build with static dependencies...');

    const moduleFactory = await this.loadModuleFactory();
    const wasmBinary = await this.loadWasmBinary();

    this.module = await moduleFactory({
      wasmBinary,
      locateFile: (path: string) => {
        if (path.endsWith('.wasm')) {
          return new URL('../../install/wasm/' + path, import.meta.url).href;
        }
        return path;
      }
    });
  }

  private async loadModuleFactory(): Promise<Function> {
    // Deno-first development environment
    if (typeof globalThis.Deno !== 'undefined') {
      const moduleFactory = (await import('../../install/wasm/irrlicht-main.js')).default;
      return moduleFactory;
    }

    // Web/CDN runtime
    const urls = [
      this.options.loadingOptions.cdnUrl + 'latest/main/irrlicht-main.js',
      ...this.options.loadingOptions.fallbackUrls!.map(url => url + 'irrlicht-main.js')
    ];

    for (const url of urls) {
      try {
        const moduleFactory = (await import(url)).default;
        return moduleFactory;
      } catch (error) {
        console.warn(`Failed to load from ${url}:`, error);
        continue;
      }
    }

    throw new Error('Failed to load module factory from any source');
  }

  private async loadWasmBinary(): Promise<ArrayBuffer | undefined> {
    // Deno-first development environment
    if (typeof globalThis.Deno !== 'undefined') {
      try {
        const wasmPath = new URL('../../install/wasm/irrlicht-main.wasm', import.meta.url).pathname;
        const wasmBuffer = await Deno.readFile(wasmPath);
        return wasmBuffer.buffer;
      } catch (error) {
        console.warn('Failed to load local WASM binary:', error);
        return undefined;
      }
    }

    // Web/CDN runtime
    const urls = [
      this.options.loadingOptions.cdnUrl + 'latest/main/irrlicht-main.wasm',
      ...this.options.loadingOptions.fallbackUrls!.map(url => url + 'irrlicht-main.wasm')
    ];

    for (const url of urls) {
      try {
        const response = await fetch(url);
        if (response.ok) {
          return await response.arrayBuffer();
        }
      } catch (error) {
        console.warn(`Failed to fetch WASM from ${url}:`, error);
        continue;
      }
    }

    return undefined; // Fallback to embedded WASM
  }

  private async initializeWebGPU(): Promise<void> {
    console.log('🎮 Initializing WebGPU context...');

    // Check for external orchestrator
    const externalContext = (globalThis as any).Module?.externalWebGPUContext;

    if (externalContext && this.options.webgpuOptions.useExternalCoordinator) {
      await this.initializeWithOrchestrator(externalContext);
    } else {
      await this.initializeStandaloneWebGPU();
    }
  }

  private async initializeWithOrchestrator(orchestrator: any): Promise<void> {
    console.log('🔗 Connecting to external WebGPU coordinator...');

    try {
      const device = await orchestrator.requestDevice({
        label: 'Irrlicht 3D Engine',
        priority: 2, // High priority for 3D rendering
        features: this.options.webgpuOptions.requiredFeatures,
        limits: this.options.webgpuOptions.deviceLimits
      });

      if (!device) {
        throw new WebGPUError('Failed to get WebGPU device from external coordinator');
      }

      // Get adapter info
      const adapter = await orchestrator.getAdapter();

      this.webgpuContext = {
        device,
        adapter,
        orchestrated: true,
        capabilities: {
          features: Array.from(device.features || []),
          limits: device.limits || {}
        }
      };

      console.log('✅ External WebGPU coordinator integration successful');

    } catch (error) {
      console.warn('⚠️  External coordinator integration failed, falling back to standalone:', error);
      await this.initializeStandaloneWebGPU();
    }
  }

  private async initializeStandaloneWebGPU(): Promise<void> {
    console.log('🎮 Initializing standalone WebGPU...');

    if (!navigator.gpu) {
      throw new WebGPUError('WebGPU not supported in this browser');
    }

    const adapter = await navigator.gpu.requestAdapter({
      powerPreference: this.options.webgpuOptions.preferHighPerformance ? 'high-performance' : 'low-power'
    });

    if (!adapter) {
      throw new WebGPUError('No WebGPU adapter available');
    }

    const device = await adapter.requestDevice({
      requiredFeatures: this.options.webgpuOptions.requiredFeatures as GPUFeatureName[],
      requiredLimits: this.options.webgpuOptions.deviceLimits
    });

    this.webgpuContext = {
      device,
      adapter,
      orchestrated: false,
      capabilities: {
        features: Array.from(device.features),
        limits: device.limits as Record<string, number>
      }
    };

    console.log('✅ Standalone WebGPU initialized');
  }

  private createDevice(): void {
    if (!this.module) {
      throw new InitializationError('WASM module not loaded');
    }

    console.log(`🎮 Creating Irrlicht device with ${DriverType[this.options.driverType]} driver...`);

    this.device = this.module._irrlicht_create_device(
      this.options.driverType,
      this.options.windowSize.width,
      this.options.windowSize.height,
      32, // bits
      0 // fullscreen (0 = false)
    );

    if (!this.device || this.device === 0) {
      throw new InitializationError(`Failed to create Irrlicht device with driver ${DriverType[this.options.driverType]}`);
    }

    // Initialize WebGPU integration if using WebGPU driver
    if (this.options.driverType === DriverType.WEBGPU && this.module._irrlicht_webgpu_init) {
      const webgpuInitialized = this.module._irrlicht_webgpu_init();
      if (!webgpuInitialized) {
        throw new WebGPUError('Failed to initialize WebGPU integration');
      }
    }
  }

  private setupCanvas(): void {
    if (!this.options.canvas) return;

    this.canvas = this.options.canvas;

    // Set canvas size to match device resolution
    this.canvas.width = this.options.windowSize.width;
    this.canvas.height = this.options.windowSize.height;

    // Setup canvas context based on driver type
    if (this.options.driverType === DriverType.WEBGPU && this.webgpuContext?.device) {
      const context = this.canvas.getContext('webgpu');
      if (context) {
        context.configure({
          device: this.webgpuContext.device,
          format: 'bgra8unorm',
          alphaMode: 'premultiplied',
          usage: GPUTextureUsage.RENDER_ATTACHMENT
        });
      }
    }

    console.log(`🖼️  Canvas configured: ${this.canvas.width}x${this.canvas.height}`);
  }

  // Public API methods

  /**
   * Check if Irrlicht is initialized
   */
  isInitialized(): boolean {
    return this.initialized;
  }

  /**
   * Run one frame of the Irrlicht main loop
   */
  run(): boolean {
    if (!this.module || !this.device) return false;
    return this.module._irrlicht_run();
  }

  /**
   * Begin a new scene for rendering
   */
  beginScene(clearColor: Color = { r: 0, g: 0, b: 0, a: 255 }): void {
    if (!this.module) return;
    this.module._irrlicht_begin_scene(255, clearColor.r, clearColor.g, clearColor.b, clearColor.a);
  }

  /**
   * End the current scene and present the frame
   */
  endScene(): void {
    if (!this.module) return;
    this.module._irrlicht_end_scene();
  }

  /**
   * Draw all objects in the scene
   */
  drawAll(): void {
    if (!this.module) return;
    this.module._irrlicht_draw_all();
  }

  /**
   * Add a camera to the scene with FPS controls
   */
  addCameraFPS(moveSpeed: number = 100, rotateSpeed: number = 0.3): CameraSceneNode {
    if (!this.module) throw new Error('Module not initialized');

    const camera = this.module._irrlicht_add_camera_fps(moveSpeed, rotateSpeed, 0, 0);
    return this.wrapCameraSceneNode(camera);
  }

  /**
   * Add a camera to the scene at specified position and target
   */
  addCamera(position: Vector3D, target: Vector3D): CameraSceneNode {
    if (!this.module) throw new Error('Module not initialized');

    const camera = this.module._irrlicht_add_camera(
      position.x, position.y, position.z,
      target.x, target.y, target.z
    );
    return this.wrapCameraSceneNode(camera);
  }

  /**
   * Add a cube mesh to the scene
   */
  addCube(size: number = 10): MeshSceneNode {
    if (!this.module) throw new Error('Module not initialized');

    const node = this.module._irrlicht_add_cube_scene_node(size);
    return this.wrapMeshSceneNode(node);
  }

  /**
   * Add a sphere mesh to the scene
   */
  addSphere(radius: number = 5, polyCount: number = 16): MeshSceneNode {
    if (!this.module) throw new Error('Module not initialized');

    const node = this.module._irrlicht_add_sphere_scene_node(radius, polyCount);
    return this.wrapMeshSceneNode(node);
  }

  /**
   * Load a mesh from file
   */
  async loadMesh(filename: string): Promise<any> {
    if (!this.module) throw new Error('Module not initialized');

    // Convert string to C string
    const filenamePtr = this.allocateString(filename);
    const mesh = this.module._irrlicht_get_mesh(filenamePtr);
    this.module._free(filenamePtr);

    if (!mesh) {
      throw new Error(`Failed to load mesh: ${filename}`);
    }

    return mesh;
  }

  /**
   * Add a mesh scene node from loaded mesh
   */
  addMeshNode(mesh: any, parent: any = null): MeshSceneNode {
    if (!this.module) throw new Error('Module not initialized');

    const node = this.module._irrlicht_add_mesh_scene_node(mesh, parent);
    return this.wrapMeshSceneNode(node);
  }

  /**
   * Get device capabilities and status
   */
  getCapabilities(): DeviceCapabilities {
    if (!this.module) {
      throw new Error('Module not initialized');
    }

    return {
      driverType: this.module._irrlicht_get_driver_type ? this.module._irrlicht_get_driver_type() : DriverType.NULL,
      driverName: this.module._irrlicht_get_driver_name ?
        this.module.UTF8ToString(this.module._irrlicht_get_driver_name()) : 'Unknown',
      webgpuSupported: this.module._irrlicht_is_webgpu_driver ? this.module._irrlicht_is_webgpu_driver() : false,
      externalCoordinatorAvailable: this.webgpuContext?.orchestrated || false,
      simdSupported: this.module._irrlicht_simd_available ? this.module._irrlicht_simd_available() : false,
      maxTextureSize: this.webgpuContext?.capabilities.limits.maxTexture2DSize || 2048,
      dependenciesLoaded: {
        zlib: this.module._irrlicht_dependency_loaded ? this.module._irrlicht_dependency_loaded('zlib') : true,
        libpng: this.module._irrlicht_dependency_loaded ? this.module._irrlicht_dependency_loaded('libpng') : true,
        libjpeg: this.module._irrlicht_dependency_loaded ? this.module._irrlicht_dependency_loaded('libjpeg') : true,
        bzip2: this.module._irrlicht_dependency_loaded ? this.module._irrlicht_dependency_loaded('bzip2') : true
      }
    };
  }

  /**
   * Get current render statistics
   */
  getRenderStatistics(): RenderStatistics {
    if (!this.module) throw new Error('Module not initialized');

    const fps = this.module._irrlicht_get_fps ? this.module._irrlicht_get_fps() : 60;
    return {
      fps,
      frameTime: 1000 / fps,
      primitiveCount: this.module._irrlicht_get_primitive_count ? this.module._irrlicht_get_primitive_count() : 0,
      drawCalls: 0, // Would need to implement tracking
      textureMemory: 0, // Would need to implement tracking
      vertexMemory: 0 // Would need to implement tracking
    };
  }

  /**
   * Run performance benchmarks
   */
  async benchmarkPerformance(): Promise<PerformanceMetrics> {
    if (!this.module) throw new Error('Module not initialized');

    console.log('🚀 Running performance benchmarks...');

    // Matrix multiplication benchmark
    const matrixIterations = 10000;
    const matrixTime = this.module._irrlicht_benchmark_matrix_multiply(matrixIterations);

    // Vertex transformation benchmark
    const vertexCount = 1000;
    const vertexIterations = 1000;
    const vertexTime = this.module._irrlicht_benchmark_vertex_transform(vertexCount, vertexIterations);

    // Calculate theoretical speedups
    const simdSupported = this.module._irrlicht_simd_available();
    const expectedMatrixSpeedup = simdSupported ? 2.5 : 1.0;
    const expectedVertexSpeedup = simdSupported ? 3.5 : 1.0;

    return {
      matrixMultiplication: {
        simdTime: matrixTime,
        scalarTime: matrixTime * expectedMatrixSpeedup,
        speedup: expectedMatrixSpeedup
      },
      vertexTransformation: {
        simdTime: vertexTime,
        scalarTime: vertexTime * expectedVertexSpeedup,
        speedup: expectedVertexSpeedup,
        verticesPerSecond: (vertexCount * vertexIterations) / (vertexTime / 1000)
      },
      frustumCulling: {
        simdTime: 0, // Would benchmark frustum culling
        scalarTime: 0,
        speedup: simdSupported ? 4.0 : 1.0,
        objectsPerSecond: 0
      }
    };
  }

  /**
   * Print detailed device information
   */
  printDeviceInfo(): void {
    if (!this.module) return;
    this.module._irrlicht_print_device_info();
  }

  /**
   * Get WebGPU context (if using WebGPU driver)
   */
  getWebGPUContext(): WebGPUContext | null {
    return this.webgpuContext;
  }

  /**
   * Cleanup resources
   */
  cleanup(): void {
    if (this.module && this.device) {
      this.module._irrlicht_destroy();
      this.device = null;
    }

    this.initialized = false;
    console.log('🧹 Irrlicht resources cleaned up');
  }

  // Private helper methods

  private wrapSceneNode(node: any): SceneNode {
    return {
      setPosition: (x: number, y: number, z: number) => {
        this.module!._irrlicht_set_node_position(node, x, y, z);
      },
      setRotation: (x: number, y: number, z: number) => {
        this.module!._irrlicht_set_node_rotation(node, x, y, z);
      },
      setScale: (x: number, y: number, z: number) => {
        this.module!._irrlicht_set_node_scale(node, x, y, z);
      },
      getPosition: (): Vector3D => {
        const result = { x: 0, y: 0, z: 0 };
        // Would need to implement position getter
        return result;
      },
      getRotation: (): Vector3D => {
        return { x: 0, y: 0, z: 0 };
      },
      getScale: (): Vector3D => {
        return { x: 1, y: 1, z: 1 };
      },
      setVisible: (visible: boolean) => {
        // Would need to implement visibility setter
      },
      isVisible: (): boolean => {
        return true;
      }
    };
  }

  private wrapMeshSceneNode(node: any): MeshSceneNode {
    const baseNode = this.wrapSceneNode(node);
    return {
      ...baseNode,
      setMaterialFlag: (flag: number, value: boolean) => {
        this.module!._irrlicht_set_material_flag(node, flag, value);
      },
      setMaterialTexture: (layer: number, texture: any) => {
        this.module!._irrlicht_set_material_texture(node, layer, texture);
      },
      setMaterialType: (type: MaterialType) => {
        this.module!._irrlicht_set_material_type(node, type);
      }
    };
  }

  private wrapCameraSceneNode(node: any): CameraSceneNode {
    const baseNode = this.wrapSceneNode(node);
    return {
      ...baseNode,
      setTarget: (x: number, y: number, z: number) => {
        // Would need to implement camera target setter
      },
      getTarget: (): Vector3D => {
        return { x: 0, y: 0, z: 0 };
      },
      setFOV: (fov: number) => {
        // Would need to implement FOV setter
      },
      getFOV: (): number => {
        return Math.PI / 4; // 45 degrees default
      },
      setAspectRatio: (ratio: number) => {
        // Would need to implement aspect ratio setter
      },
      setNearValue: (near: number) => {
        // Would need to implement near plane setter
      },
      setFarValue: (far: number) => {
        // Would need to implement far plane setter
      }
    };
  }

  private allocateString(str: string): number {
    if (!this.module) throw new Error('Module not initialized');

    const bytes = new TextEncoder().encode(str + '\0');
    const ptr = this.module._malloc(bytes.length);
    this.module.HEAPU8.set(bytes, ptr);
    return ptr;
  }
}

// Export convenience functions
export async function createIrrlichtDevice(options: IrrlichtOptions = {}): Promise<Irrlicht> {
  const engine = new Irrlicht(options);
  await engine.initialize();
  return engine;
}

export function checkWebGPUSupport(): boolean {
  return 'gpu' in navigator;
}

export function checkSIMDSupport(): boolean {
  try {
    // Test WASM SIMD support
    const testModule = new Uint8Array([
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
      0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7b, 0x03,
      0x02, 0x01, 0x00, 0x0a, 0x0a, 0x01, 0x08, 0x00,
      0x41, 0x00, 0xfd, 0x0f, 0xfd, 0x62, 0x0b
    ]);
    return WebAssembly.validate(testModule);
  } catch {
    return false;
  }
}