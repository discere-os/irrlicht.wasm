// Irrlicht Engine WebGPU Vertex Shader
// Copyright (c) 2002-2012 Nikolaus Gebhardt
// Copyright (c) 2025 Superstruct Ltd, New Zealand
// Licensed under the zlib/libpng license

struct IrrlichtTransforms {
  worldMatrix: mat4x4f,
  viewMatrix: mat4x4f,
  projectionMatrix: mat4x4f,
  normalMatrix: mat3x3f,
  _padding: vec4f,
}

struct IrrlichtMaterial {
  diffuseColor: vec4f,
  specularColor: vec4f,
  emissiveColor: vec4f,
  shininess: f32,
  materialType: u32,
  lightingEnabled: u32,
  _padding: f32,
}

struct IrrlichtLighting {
  ambientColor: vec3f,
  lightCount: u32,
  lights: array<IrrlichtLight, 8>,
}

struct IrrlichtLight {
  position: vec3f,
  type: u32,        // 0=directional, 1=point, 2=spot
  direction: vec3f,
  range: f32,
  diffuseColor: vec3f,
  specularColor: vec3f,
  attenuation: vec3f,   // constant, linear, quadratic
  spotData: vec2f,      // inner/outer cone angles
}

@group(0) @binding(0) var<uniform> transforms: IrrlichtTransforms;
@group(0) @binding(1) var<uniform> material: IrrlichtMaterial;
@group(0) @binding(2) var<uniform> lighting: IrrlichtLighting;
@group(0) @binding(3) var diffuseTexture: texture_2d<f32>;
@group(0) @binding(4) var textureSampler: sampler;

// Vertex input structures for different vertex types
struct StandardVertexInput {
  @location(0) position: vec3f,
  @location(1) normal: vec3f,
  @location(2) texCoord: vec2f,
  @location(3) color: vec4f,
}

struct TwoTexCoordsVertexInput {
  @location(0) position: vec3f,
  @location(1) normal: vec3f,
  @location(2) texCoord1: vec2f,
  @location(3) texCoord2: vec2f,
  @location(4) color: vec4f,
}

struct TangentVertexInput {
  @location(0) position: vec3f,
  @location(1) normal: vec3f,
  @location(2) texCoord: vec2f,
  @location(3) tangent: vec3f,
  @location(4) binormal: vec3f,
  @location(5) color: vec4f,
}

struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) worldPos: vec3f,
  @location(1) normal: vec3f,
  @location(2) texCoord: vec2f,
  @location(3) color: vec4f,
  @location(4) viewPos: vec3f,
  @location(5) tangent: vec3f,
  @location(6) binormal: vec3f,
}

// Standard vertex shader for S3DVertex
@vertex fn standardVertex(input: StandardVertexInput) -> VertexOutput {
  let worldPos = transforms.worldMatrix * vec4f(input.position, 1.0);
  let viewPos = transforms.viewMatrix * worldPos;
  let clipPos = transforms.projectionMatrix * viewPos;

  var output: VertexOutput;
  output.position = clipPos;
  output.worldPos = worldPos.xyz;
  output.normal = normalize(transforms.normalMatrix * input.normal);
  output.texCoord = input.texCoord;
  output.color = input.color;
  output.viewPos = viewPos.xyz;
  // Default tangent space for standard vertices
  output.tangent = vec3f(1.0, 0.0, 0.0);
  output.binormal = vec3f(0.0, 1.0, 0.0);

  return output;
}

// Two texture coordinates vertex shader for S3DVertex2TCoords
@vertex fn twoTexCoordsVertex(input: TwoTexCoordsVertexInput) -> VertexOutput {
  let worldPos = transforms.worldMatrix * vec4f(input.position, 1.0);
  let viewPos = transforms.viewMatrix * worldPos;
  let clipPos = transforms.projectionMatrix * viewPos;

  var output: VertexOutput;
  output.position = clipPos;
  output.worldPos = worldPos.xyz;
  output.normal = normalize(transforms.normalMatrix * input.normal);
  output.texCoord = input.texCoord1; // Use first texture coordinate
  output.color = input.color;
  output.viewPos = viewPos.xyz;
  output.tangent = vec3f(1.0, 0.0, 0.0);
  output.binormal = vec3f(0.0, 1.0, 0.0);

  return output;
}

// Tangent space vertex shader for S3DVertexTangents
@vertex fn tangentVertex(input: TangentVertexInput) -> VertexOutput {
  let worldPos = transforms.worldMatrix * vec4f(input.position, 1.0);
  let viewPos = transforms.viewMatrix * worldPos;
  let clipPos = transforms.projectionMatrix * viewPos;

  var output: VertexOutput;
  output.position = clipPos;
  output.worldPos = worldPos.xyz;
  output.normal = normalize(transforms.normalMatrix * input.normal);
  output.texCoord = input.texCoord;
  output.color = input.color;
  output.viewPos = viewPos.xyz;
  output.tangent = normalize(transforms.normalMatrix * input.tangent);
  output.binormal = normalize(transforms.normalMatrix * input.binormal);

  return output;
}

// 2D vertex shader for GUI and HUD elements
struct Vertex2DInput {
  @location(0) position: vec2f,
  @location(1) texCoord: vec2f,
  @location(2) color: vec4f,
}

struct Vertex2DOutput {
  @builtin(position) position: vec4f,
  @location(0) texCoord: vec2f,
  @location(1) color: vec4f,
}

@vertex fn vertex2D(input: Vertex2DInput) -> Vertex2DOutput {
  // Convert from screen coordinates to clip space
  let clipX = (input.position.x / 1024.0) * 2.0 - 1.0; // Assume 1024x768 for now
  let clipY = -((input.position.y / 768.0) * 2.0 - 1.0); // Flip Y

  var output: Vertex2DOutput;
  output.position = vec4f(clipX, clipY, 0.0, 1.0);
  output.texCoord = input.texCoord;
  output.color = input.color;

  return output;
}

// Utility functions
fn calculateTangentBinormal(normal: vec3f) -> mat3x3f {
  // Generate tangent and binormal from normal
  let tangent = select(
    normalize(cross(normal, vec3f(0.0, 1.0, 0.0))),
    normalize(cross(normal, vec3f(1.0, 0.0, 0.0))),
    abs(normal.y) > 0.9
  );
  let binormal = cross(normal, tangent);

  return mat3x3f(tangent, binormal, normal);
}

// Shadow mapping vertex shader
@vertex fn shadowMapVertex(input: StandardVertexInput) -> @builtin(position) vec4f {
  let worldPos = transforms.worldMatrix * vec4f(input.position, 1.0);
  return transforms.projectionMatrix * transforms.viewMatrix * worldPos;
}

// Instanced rendering vertex shader
struct InstanceData {
  transform: mat4x4f,
  color: vec4f,
}

@group(1) @binding(0) var<storage, read> instances: array<InstanceData>;

@vertex fn instancedVertex(
  input: StandardVertexInput,
  @builtin(instance_index) instanceIndex: u32
) -> VertexOutput {
  let instance = instances[instanceIndex];
  let localPos = vec4f(input.position, 1.0);
  let worldPos = transforms.worldMatrix * instance.transform * localPos;
  let viewPos = transforms.viewMatrix * worldPos;
  let clipPos = transforms.projectionMatrix * viewPos;

  var output: VertexOutput;
  output.position = clipPos;
  output.worldPos = worldPos.xyz;
  output.normal = normalize(transforms.normalMatrix * input.normal);
  output.texCoord = input.texCoord;
  output.color = input.color * instance.color;
  output.viewPos = viewPos.xyz;
  output.tangent = vec3f(1.0, 0.0, 0.0);
  output.binormal = vec3f(0.0, 1.0, 0.0);

  return output;
}

// Particle system vertex shader
struct ParticleInput {
  @location(0) position: vec3f,
  @location(1) size: f32,
  @location(2) color: vec4f,
  @location(3) texCoord: vec2f,
}

@vertex fn particleVertex(input: ParticleInput) -> VertexOutput {
  // Billboard the particle to face the camera
  let viewMatrix = transforms.viewMatrix;
  let right = vec3f(viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0]);
  let up = vec3f(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]);

  let billboardPos = input.position + right * input.texCoord.x * input.size + up * input.texCoord.y * input.size;
  let worldPos = vec4f(billboardPos, 1.0);
  let viewPos = transforms.viewMatrix * worldPos;
  let clipPos = transforms.projectionMatrix * viewPos;

  var output: VertexOutput;
  output.position = clipPos;
  output.worldPos = worldPos.xyz;
  output.normal = vec3f(0.0, 0.0, 1.0); // Face camera
  output.texCoord = input.texCoord;
  output.color = input.color;
  output.viewPos = viewPos.xyz;
  output.tangent = right;
  output.binormal = up;

  return output;
}