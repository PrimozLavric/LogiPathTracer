#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "uniforms.glsl"

#include "../heitz/BSDF.glsl"
#include "../heitz/interaction_type.glsl"
#include "../basic/BSDF.glsl"

#define RUSSIAN_ROULETTE_DEPTH 2
#define MAX_TRACE_DEPTH 10

layout(location = 0) rayPayloadInNV RayPayload payload;
hitAttributeNV vec3 attribs;

//#define USE_MICROFACET

void main() {
  seed = payload.seed;
  uint vertexOffset = materials[gl_InstanceID].verticesOffset + gl_PrimitiveID * 3;

  const vec3 bary = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
  vec3 intersectionPosition = gl_WorldRayOriginNV + gl_HitTNV * gl_WorldRayDirectionNV;
  vec2 uv = bary.x * vertices[vertexOffset].uv + bary.y * vertices[vertexOffset + 1].uv + bary.z * vertices[vertexOffset + 2].uv;

  vec3 normal = normalize(mat3(gl_ObjectToWorldNV) * (bary.x * vertices[vertexOffset].normal + bary.y * vertices[vertexOffset + 1].normal + bary.z * vertices[vertexOffset + 2].normal));

  vec4 baseColorFactor = materials[gl_InstanceID].baseColorFactor;
  vec3 emissionFactor = materials[gl_InstanceID].emissionFactor;
  float roughnessFactor = max(materials[gl_InstanceID].roughnessFactor, 0.001f);
  float metallicFactor = materials[gl_InstanceID].metallicFactor;
  float transmissionFactor = materials[gl_InstanceID].transmissionFactor;
  float ior = materials[gl_InstanceID].ior;

  // Color texture.
  if (materials[gl_InstanceID].colorTexture != 0XFFFFFFFF) {
    baseColorFactor *= texture(textures[materials[gl_InstanceID].colorTexture], uv).xyzw;
  }

  // Emission texture.
  if (materials[gl_InstanceID].emissionTexture != 0XFFFFFFFF) {
    emissionFactor *= texture(textures[materials[gl_InstanceID].emissionTexture], uv).xyz;
  }

  if (materials[gl_InstanceID].metallicRoughnessTexture != 0XFFFFFFFF) {
    vec4 metallicRoughnessSample = texture(textures[materials[gl_InstanceID].metallicRoughnessTexture], uv);
    metallicFactor *= metallicRoughnessSample.b;
    roughnessFactor *= metallicRoughnessSample.g;
  }

  if (materials[gl_InstanceID].transmissionTexture != 0XFFFFFFFF) {
    transmissionFactor *= texture(textures[materials[gl_InstanceID].transmissionTexture], uv).x;
  }

  baseColorFactor = SRGBToLinear(baseColorFactor);

  // Determine interaction type based on the emission, roughness and metallic and transmission factor.
  uint interaction = determineMicrofacetInteractionType(metallicFactor, transmissionFactor);

  // Apple emission.
  payload.accColor += payload.mask * emissionFactor;

  // Compute orthonormal basis.
  vec3 ffNormal = (dot(normal, -gl_WorldRayDirectionNV) > 0.0f) ? normal : normal * -1.0f;
  vec3 u = normalize(cross((abs(ffNormal.x) > 0.1f ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f)), ffNormal));
  vec3 v = cross(ffNormal, u);

  if (materials[gl_InstanceID].normalTexture != 0XFFFFFFFF) {
    vec3 tangentNormal = normalize(texture(textures[materials[gl_InstanceID].normalTexture], uv).xyz * 2.0 - 1.0);
    ffNormal = normalize(mat3(u, v, ffNormal) * tangentNormal);
    u = normalize(cross((abs(ffNormal.x) > 0.1f ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f)), ffNormal));
    v = cross(ffNormal, u);
  }

  vec3 viewDir;
  vec3 lightDir;
  viewDir.x = dot(-gl_WorldRayDirectionNV, u);
  viewDir.y = dot(-gl_WorldRayDirectionNV, v);
  viewDir.z = dot(-gl_WorldRayDirectionNV, ffNormal);


  if (interaction == kDiff) {
    #ifdef USE_MICROFACET
      payload.mask *= DiffuseBSDF(baseColorFactor.xyz, viewDir, roughnessFactor, lightDir);
    #else
      payload.mask *= BasicDiffuseBRDF(baseColorFactor.xyz, viewDir, lightDir);
    #endif
  } else if (interaction == kMetallic) {
      #ifdef USE_MICROFACET
        payload.mask *= ConductorBRDF(baseColorFactor.xyz, viewDir, roughnessFactor, lightDir);
      #else
        payload.mask *= BasicSpecularBRDF(baseColorFactor.xyz, viewDir, lightDir);
      #endif
  } else if (interaction == kTrans) {
    bool outside = dot(normal, -gl_WorldRayDirectionNV) > 0.0f;

    #ifdef USE_MICROFACET
      payload.mask *= DielectricBSDF(baseColorFactor.xyz, viewDir, roughnessFactor, transmissionFactor, ior, lightDir, outside);
    #else
      payload.mask *= BasicTransmittanceBRDF(baseColorFactor.xyz, viewDir, transmissionFactor, ior, outside, lightDir);
    #endif
  }

  lightDir = lightDir.x * u + lightDir.y * v + lightDir.z * ffNormal;

  // Increment depth
  payload.depth += 1;

  float maskLength = length(payload.mask);
  if (maskLength < 0.5 && payload.depth > RUSSIAN_ROULETTE_DEPTH) {
      float q = max(0.05f, 1.0f - maskLength);
      if (rand() < q) {
          return;
      }
      payload.mask /= 1.0f - q;
  }

  payload.seed = seed;
  if (payload.depth < MAX_TRACE_DEPTH) {
    traceNV(accelerator, gl_RayFlagsOpaqueNV, 0xff, 0, 0, 0, gl_WorldRayOriginNV + gl_HitTNV * gl_WorldRayDirectionNV, 0.00001, normalize(lightDir), 10000.0, 0);
  }
}