/**
 * Type definitions for Irrlicht.wasm
 * Copyright (c) 2002-2012 Nikolaus Gebhardt
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under the zlib/libpng license
 */

// Driver types
export enum DriverType {
  NULL = 0,
  SOFTWARE = 1,
  BURNINGSVIDEO = 2,
  DIRECT3D9 = 4,
  OPENGL = 5,
  WEBGPU = 6
}

// Material types
export enum MaterialType {
  SOLID = 0,
  SOLID_2_LAYER = 1,
  LIGHTMAP = 2,
  LIGHTMAP_ADD = 3,
  LIGHTMAP_M2 = 4,
  LIGHTMAP_M4 = 5,
  LIGHTMAP_LIGHTING = 6,
  LIGHTMAP_LIGHTING_M2 = 7,
  LIGHTMAP_LIGHTING_M4 = 8,
  DETAIL_MAP = 9,
  SPHERE_MAP = 10,
  REFLECTION_2_LAYER = 11,
  TRANSPARENT_ADD_COLOR = 12,
  TRANSPARENT_ALPHA_CHANNEL = 13,
  TRANSPARENT_ALPHA_CHANNEL_REF = 14,
  TRANSPARENT_VERTEX_ALPHA = 15,
  TRANSPARENT_REFLECTION_2_LAYER = 16,
  NORMAL_MAP_SOLID = 17,
  NORMAL_MAP_TRANSPARENT_ADD_COLOR = 18,
  NORMAL_MAP_TRANSPARENT_VERTEX_ALPHA = 19,
  PARALLAX_MAP_SOLID = 20,
  PARALLAX_MAP_TRANSPARENT_ADD_COLOR = 21,
  PARALLAX_MAP_TRANSPARENT_VERTEX_ALPHA = 22,
  ONETEXTURE_BLEND = 23
}

// Primitive types
export enum PrimitiveType {
  POINTS = 0,
  LINE_STRIP = 1,
  LINE_LOOP = 2,
  LINES = 3,
  TRIANGLE_STRIP = 4,
  TRIANGLE_FAN = 5,
  TRIANGLES = 6,
  QUAD_STRIP = 7,
  QUADS = 8,
  POLYGON = 9
}

// Color formats
export enum ColorFormat {
  A1R5G5B5 = 0,
  R5G6B5 = 1,
  R8G8B8 = 2,
  A8R8G8B8 = 3,
  DXT1 = 4,
  DXT2 = 5,
  DXT3 = 6,
  DXT4 = 7,
  DXT5 = 8,
  R16F = 9,
  G16R16F = 10,
  A16B16G16R16F = 11,
  R32F = 12,
  G32R32F = 13,
  A32B32G32R32F = 14,
  UNKNOWN = 15
}

// Vector3D type
export interface Vector3D {
  x: number;
  y: number;
  z: number;
}

// Color type
export interface Color {
  r: number;
  g: number;
  b: number;
  a: number;
}

// Dimension2D type
export interface Dimension2D {
  width: number;
  height: number;
}

// Matrix4 type
export type Matrix4 = Float32Array; // 16 elements

// Creation parameters
export interface CreationParameters {
  driverType: DriverType;
  windowSize: Dimension2D;
  bits: number;
  fullscreen: boolean;
  stencilBuffer: boolean;
  vsync: boolean;
  deviceType: string;
}

// WebGPU-specific options
export interface WebGPUOptions {
  useExternalCoordinator?: boolean;
  preferHighPerformance?: boolean;
  requiredFeatures?: string[];
  deviceLimits?: {
    maxTextureSize?: number;
    maxUniformBufferSize?: number;
    maxStorageBufferSize?: number;
  };
}

// Irrlicht device options
export interface IrrlichtOptions {
  canvas?: HTMLCanvasElement;
  driverType?: DriverType;
  windowSize?: Dimension2D;
  webgpuOptions?: WebGPUOptions;
  loadingOptions?: LoadingOptions;
  simdOptimizations?: boolean;
}

// Loading options
export interface LoadingOptions {
  cdnUrl?: string;
  fallbackUrls?: string[];
  useSideModules?: boolean;
  cachingEnabled?: boolean;
  maxMemoryMB?: number;
  timeoutMs?: number;
}

// Scene node interface
export interface SceneNode {
  setPosition(x: number, y: number, z: number): void;
  setRotation(x: number, y: number, z: number): void;
  setScale(x: number, y: number, z: number): void;
  getPosition(): Vector3D;
  getRotation(): Vector3D;
  getScale(): Vector3D;
  setVisible(visible: boolean): void;
  isVisible(): boolean;
}

// Mesh scene node interface
export interface MeshSceneNode extends SceneNode {
  setMaterialFlag(flag: number, value: boolean): void;
  setMaterialTexture(layer: number, texture: Texture): void;
  setMaterialType(type: MaterialType): void;
}

// Camera scene node interface
export interface CameraSceneNode extends SceneNode {
  setTarget(x: number, y: number, z: number): void;
  getTarget(): Vector3D;
  setFOV(fov: number): void;
  getFOV(): number;
  setAspectRatio(ratio: number): void;
  setNearValue(near: number): void;
  setFarValue(far: number): void;
}

// Light scene node interface
export interface LightSceneNode extends SceneNode {
  setLightType(type: number): void;
  setRadius(radius: number): void;
  getLightData(): LightData;
}

// Light data
export interface LightData {
  position: Vector3D;
  direction: Vector3D;
  diffuseColor: Color;
  specularColor: Color;
  attenuation: Vector3D;
  range: number;
  type: number;
}

// Texture interface
export interface Texture {
  getSize(): Dimension2D;
  getColorFormat(): ColorFormat;
  getPitch(): number;
  hasAlpha(): boolean;
  hasMipMaps(): boolean;
}

// Animated mesh interface
export interface AnimatedMesh {
  getFrameCount(): number;
  getMesh(frame: number): Mesh;
  getBoundingBox(): BoundingBox;
}

// Static mesh interface
export interface Mesh {
  getMeshBufferCount(): number;
  getMeshBuffer(index: number): MeshBuffer;
  getBoundingBox(): BoundingBox;
}

// Mesh buffer interface
export interface MeshBuffer {
  getVertexCount(): number;
  getIndexCount(): number;
  getVertexType(): number;
  getVertices(): ArrayBuffer;
  getIndices(): ArrayBuffer;
  getBoundingBox(): BoundingBox;
}

// Bounding box
export interface BoundingBox {
  min: Vector3D;
  max: Vector3D;
}

// Device capabilities
export interface DeviceCapabilities {
  driverType: DriverType;
  driverName: string;
  webgpuSupported: boolean;
  externalCoordinatorAvailable: boolean;
  simdSupported: boolean;
  maxTextureSize: number;
  dependenciesLoaded: {
    zlib: boolean;
    libpng: boolean;
    libjpeg: boolean;
    bzip2: boolean;
  };
}

// Render statistics
export interface RenderStatistics {
  fps: number;
  frameTime: number;
  primitiveCount: number;
  drawCalls: number;
  textureMemory: number;
  vertexMemory: number;
}

// Animation data
export interface AnimationData {
  name: string;
  duration: number;
  frameRate: number;
  loop: boolean;
}

// Scene graph node data
export interface SceneNodeData {
  id: number;
  name: string;
  type: string;
  position: Vector3D;
  rotation: Vector3D;
  scale: Vector3D;
  visible: boolean;
  material?: MaterialData;
}

// Material data
export interface MaterialData {
  type: MaterialType;
  diffuseColor: Color;
  specularColor: Color;
  emissiveColor: Color;
  shininess: number;
  textures: string[];
  flags: MaterialFlags;
}

// Material flags
export interface MaterialFlags {
  lighting: boolean;
  wireframe: boolean;
  gouraudShading: boolean;
  bilinearFilter: boolean;
  trilinearFilter: boolean;
  anisotropicFilter: boolean;
  fog: boolean;
  normalizeNormals: boolean;
  zWrite: boolean;
  backfaceCulling: boolean;
  frontfaceCulling: boolean;
}

// Error types
export class IrrlichtError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'IrrlichtError';
  }
}

export class WebGPUError extends IrrlichtError {
  constructor(message: string) {
    super(`WebGPU Error: ${message}`);
    this.name = 'WebGPUError';
  }
}

export class InitializationError extends IrrlichtError {
  constructor(message: string) {
    super(`Initialization Error: ${message}`);
    this.name = 'InitializationError';
  }
}

export class DependencyError extends IrrlichtError {
  constructor(message: string) {
    super(`Dependency Error: ${message}`);
    this.name = 'DependencyError';
  }
}

// Performance metrics
export interface PerformanceMetrics {
  matrixMultiplication: {
    simdTime: number;
    scalarTime: number;
    speedup: number;
  };
  vertexTransformation: {
    simdTime: number;
    scalarTime: number;
    speedup: number;
    verticesPerSecond: number;
  };
  frustumCulling: {
    simdTime: number;
    scalarTime: number;
    speedup: number;
    objectsPerSecond: number;
  };
}

// WebGPU context information
export interface WebGPUContext {
  device: GPUDevice | null;
  adapter: GPUAdapter | null;
  orchestrated: boolean;
  capabilities: {
    features: string[];
    limits: Record<string, number>;
  };
}

// Module loading result
export interface ModuleLoadResult {
  success: boolean;
  module: any;
  error?: string;
  loadTime: number;
  dependencies: string[];
}

// Irrlicht module interface
export interface IrrlichtModule {
  // Core functions
  _irrlicht_create_device: (driverType: number, width: number, height: number, bits: number, fullscreen: number) => any;
  _irrlicht_destroy: () => void;
  _irrlicht_run: () => boolean;
  _irrlicht_get_video_driver: () => any;
  _irrlicht_get_scene_manager: () => any;
  _irrlicht_get_gui_environment: () => any;

  // Scene management
  _irrlicht_add_camera_fps: (moveSpeed: number, rotateSpeed: number, keyMapArray: number, keyMapSize: number) => any;
  _irrlicht_add_camera: (posX: number, posY: number, posZ: number, targetX: number, targetY: number, targetZ: number) => any;
  _irrlicht_add_cube_scene_node: (size: number) => any;
  _irrlicht_add_sphere_scene_node: (radius: number, polyCount: number) => any;
  _irrlicht_get_mesh: (filename: string) => any;
  _irrlicht_add_mesh_scene_node: (mesh: any, parent: any) => any;

  // Rendering
  _irrlicht_begin_scene: (clearFlag: number, r: number, g: number, b: number, a: number) => void;
  _irrlicht_end_scene: () => void;
  _irrlicht_draw_all: () => void;

  // Utility
  _irrlicht_get_version: () => string;
  _irrlicht_get_driver_type?: () => number;
  _irrlicht_get_driver_name?: () => string;
  _irrlicht_is_webgpu_driver?: () => boolean;
  _irrlicht_webgpu_init?: () => boolean;
  _irrlicht_print_device_info: () => void;

  // Performance
  _irrlicht_benchmark_matrix_multiply: (iterations: number) => number;
  _irrlicht_benchmark_vertex_transform: (vertexCount: number, iterations: number) => number;
  _irrlicht_simd_available: () => boolean;

  // Emscripten runtime
  cwrap: (name: string, returnType: string, argTypes: string[]) => Function;
  ccall: (name: string, returnType: string, argTypes: string[], args: any[]) => any;
  UTF8ToString: (ptr: number) => string;
  stringToUTF8: (str: string, buffer: number, maxBytes: number) => void;
  _malloc: (size: number) => number;
  _free: (ptr: number) => void;
  HEAPU8: Uint8Array;
  HEAPU32: Uint32Array;
  HEAPF32: Float32Array;
}