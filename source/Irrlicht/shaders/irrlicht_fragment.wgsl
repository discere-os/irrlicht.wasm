// Irrlicht Engine WebGPU Fragment Shader
// Copyright (c) 2002-2012 Nikolaus Gebhardt
// Copyright (c) 2025 Superstruct Ltd, New Zealand
// Licensed under the zlib/libpng license

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
  type: u32,
  direction: vec3f,
  range: f32,
  diffuseColor: vec3f,
  specularColor: vec3f,
  attenuation: vec3f,
  spotData: vec2f,
}

@group(0) @binding(1) var<uniform> material: IrrlichtMaterial;
@group(0) @binding(2) var<uniform> lighting: IrrlichtLighting;
@group(0) @binding(3) var diffuseTexture: texture_2d<f32>;
@group(0) @binding(4) var textureSampler: sampler;

// Material type constants (matching Irrlicht's E_MATERIAL_TYPE)
const EMT_SOLID: u32 = 0u;
const EMT_SOLID_2_LAYER: u32 = 1u;
const EMT_LIGHTMAP: u32 = 2u;
const EMT_LIGHTMAP_ADD: u32 = 3u;
const EMT_LIGHTMAP_M2: u32 = 4u;
const EMT_LIGHTMAP_M4: u32 = 5u;
const EMT_LIGHTMAP_LIGHTING: u32 = 6u;
const EMT_LIGHTMAP_LIGHTING_M2: u32 = 7u;
const EMT_LIGHTMAP_LIGHTING_M4: u32 = 8u;
const EMT_DETAIL_MAP: u32 = 9u;
const EMT_SPHERE_MAP: u32 = 10u;
const EMT_REFLECTION_2_LAYER: u32 = 11u;
const EMT_TRANSPARENT_ADD_COLOR: u32 = 12u;
const EMT_TRANSPARENT_ALPHA_CHANNEL: u32 = 13u;
const EMT_TRANSPARENT_ALPHA_CHANNEL_REF: u32 = 14u;
const EMT_TRANSPARENT_VERTEX_ALPHA: u32 = 15u;
const EMT_TRANSPARENT_REFLECTION_2_LAYER: u32 = 16u;
const EMT_NORMAL_MAP_SOLID: u32 = 17u;
const EMT_NORMAL_MAP_TRANSPARENT_ADD_COLOR: u32 = 18u;
const EMT_NORMAL_MAP_TRANSPARENT_VERTEX_ALPHA: u32 = 19u;
const EMT_PARALLAX_MAP_SOLID: u32 = 20u;
const EMT_PARALLAX_MAP_TRANSPARENT_ADD_COLOR: u32 = 21u;
const EMT_PARALLAX_MAP_TRANSPARENT_VERTEX_ALPHA: u32 = 22u;
const EMT_ONETEXTURE_BLEND: u32 = 23u;

// Standard fragment shader with Irrlicht-compatible lighting
@fragment fn standardFragment(input: VertexOutput) -> @location(0) vec4f {
  // Sample diffuse texture
  let textureColor = textureSample(diffuseTexture, textureSampler, input.texCoord);
  let baseColor = textureColor * input.color * material.diffuseColor;

  // Handle different material types
  switch (material.materialType) {
    case EMT_SOLID: {
      return calculateLitColor(input, baseColor);
    }
    case EMT_TRANSPARENT_ALPHA_CHANNEL: {
      let litColor = calculateLitColor(input, baseColor);
      return vec4f(litColor.rgb, baseColor.a);
    }
    case EMT_TRANSPARENT_ADD_COLOR: {
      let litColor = calculateLitColor(input, baseColor);
      return vec4f(litColor.rgb, 1.0); // Additive blending
    }
    case EMT_TRANSPARENT_VERTEX_ALPHA: {
      let litColor = calculateLitColor(input, baseColor);
      return vec4f(litColor.rgb, input.color.a);
    }
    default: {
      return calculateLitColor(input, baseColor);
    }
  }
}

// Lighting calculation compatible with Irrlicht's lighting model
fn calculateLitColor(input: VertexOutput, baseColor: vec4f) -> vec4f {
  if (material.lightingEnabled == 0u) {
    return baseColor;
  }

  let normal = normalize(input.normal);
  var finalColor = lighting.ambientColor * baseColor.rgb;

  // Add contribution from each light
  for (var i: u32 = 0u; i < lighting.lightCount; i++) {
    let light = lighting.lights[i];
    let lightContrib = calculateLightContribution(input.worldPos, normal, input.viewPos, light);
    finalColor += lightContrib * baseColor.rgb;
  }

  // Add emissive color
  finalColor += material.emissiveColor.rgb;

  return vec4f(finalColor, baseColor.a);
}

fn calculateLightContribution(worldPos: vec3f, normal: vec3f, viewPos: vec3f, light: IrrlichtLight) -> vec3f {
  var lightDir: vec3f;
  var attenuation = 1.0;

  // Calculate light direction and attenuation based on light type
  switch (light.type) {
    case 0u: { // Directional light
      lightDir = normalize(-light.direction);
    }
    case 1u: { // Point light
      let lightToPos = worldPos - light.position;
      let distance = length(lightToPos);
      lightDir = normalize(-lightToPos);

      if (distance > light.range) {
        return vec3f(0.0);
      }

      // Irrlicht's attenuation model
      attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * distance + light.attenuation.z * distance * distance);
    }
    case 2u: { // Spot light
      let lightToPos = worldPos - light.position;
      let distance = length(lightToPos);
      lightDir = normalize(-lightToPos);

      if (distance > light.range) {
        return vec3f(0.0);
      }

      // Distance attenuation
      attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * distance + light.attenuation.z * distance * distance);

      // Spot cone attenuation
      let spotDir = normalize(light.direction);
      let spotFactor = dot(-lightDir, spotDir);
      let innerCone = cos(light.spotData.x);
      let outerCone = cos(light.spotData.y);

      if (spotFactor < outerCone) {
        return vec3f(0.0);
      }

      let spotAttenuation = smoothstep(outerCone, innerCone, spotFactor);
      attenuation *= spotAttenuation;
    }
    default: {
      return vec3f(0.0);
    }
  }

  // Lambertian diffuse lighting
  let NdotL = max(dot(normal, lightDir), 0.0);
  let diffuse = light.diffuseColor * NdotL;

  // Blinn-Phong specular lighting
  var specular = vec3f(0.0);
  if (NdotL > 0.0 && material.shininess > 0.0) {
    let viewDir = normalize(-viewPos);
    let halfDir = normalize(lightDir + viewDir);
    let NdotH = max(dot(normal, halfDir), 0.0);
    let specularPower = pow(NdotH, material.shininess);
    specular = light.specularColor * material.specularColor.rgb * specularPower;
  }

  return (diffuse + specular) * attenuation;
}

// Specialized fragment shaders for different material types

// Normal mapping fragment shader
@group(0) @binding(5) var normalTexture: texture_2d<f32>;

@fragment fn normalMapFragment(input: VertexOutput) -> @location(0) vec4f {
  let textureColor = textureSample(diffuseTexture, textureSampler, input.texCoord);
  let normalMapColor = textureSample(normalTexture, textureSampler, input.texCoord);

  // Convert normal map from [0,1] to [-1,1]
  let tangentNormal = normalMapColor.rgb * 2.0 - 1.0;

  // Transform to world space using TBN matrix
  let T = normalize(input.tangent);
  let B = normalize(input.binormal);
  let N = normalize(input.normal);
  let TBN = mat3x3f(T, B, N);

  let worldNormal = normalize(TBN * tangentNormal);

  // Create modified input with perturbed normal
  var modifiedInput = input;
  modifiedInput.normal = worldNormal;

  let baseColor = textureColor * input.color * material.diffuseColor;
  return calculateLitColor(modifiedInput, baseColor);
}

// Environment mapping fragment shader
@group(0) @binding(5) var envTexture: texture_cube<f32>;

@fragment fn environmentMapFragment(input: VertexOutput) -> @location(0) vec4f {
  let textureColor = textureSample(diffuseTexture, textureSampler, input.texCoord);

  // Calculate reflection vector
  let viewDir = normalize(input.worldPos - input.viewPos);
  let reflectionDir = reflect(viewDir, normalize(input.normal));

  // Sample environment map
  let envColor = textureSample(envTexture, textureSampler, reflectionDir);

  // Blend with base texture
  let baseColor = textureColor * input.color * material.diffuseColor;
  let litColor = calculateLitColor(input, baseColor);

  // Mix with environment reflection
  let reflectivity = material.specularColor.a;
  return vec4f(mix(litColor.rgb, envColor.rgb, reflectivity), baseColor.a);
}

// Transparent fragment shader with alpha testing
@fragment fn transparentFragment(input: VertexOutput) -> @location(0) vec4f {
  let textureColor = textureSample(diffuseTexture, textureSampler, input.texCoord);
  let baseColor = textureColor * input.color * material.diffuseColor;

  // Alpha testing for EMT_TRANSPARENT_ALPHA_CHANNEL_REF
  if (material.materialType == EMT_TRANSPARENT_ALPHA_CHANNEL_REF && baseColor.a < 0.5) {
    discard;
  }

  return calculateLitColor(input, baseColor);
}

// Simple unlit fragment shader
@fragment fn unlitFragment(input: VertexOutput) -> @location(0) vec4f {
  let textureColor = textureSample(diffuseTexture, textureSampler, input.texCoord);
  return textureColor * input.color * material.diffuseColor;
}

// 2D fragment shader for GUI
@fragment fn fragment2D(input: Vertex2DOutput) -> @location(0) vec4f {
  let textureColor = textureSample(diffuseTexture, textureSampler, input.texCoord);
  return textureColor * input.color;
}

// Post-processing fragment shader
@fragment fn postProcessFragment(@location(0) texCoord: vec2f) -> @location(0) vec4f {
  return textureSample(diffuseTexture, textureSampler, texCoord);
}

// Shadow mapping fragment shader
@fragment fn shadowMapFragment(input: VertexOutput) -> @location(0) vec4f {
  // Output depth to shadow map
  return vec4f(input.position.z, input.position.z, input.position.z, 1.0);
}