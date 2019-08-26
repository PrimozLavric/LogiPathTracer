#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "uniforms.glsl"

#include "../heitz/BSDF.glsl"
#include "../heitz/interaction_type.glsl"

#define RUSSIAN_ROULETTE_DEPTH 2
#define MAX_TRACE_DEPTH 10

layout(location = 0) rayPayloadInNV RayPayload payload;
hitAttributeNV vec3 attribs;

void main() {
  seed = payload.seed;
  const vec3 bary = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
  uint vertexOffset = materials[gl_InstanceID].verticesOffset + gl_PrimitiveID * 3;

  vec3 normal = normalize(mat3(gl_ObjectToWorldNV) * (bary.x * vertices[vertexOffset].normal + bary.y * vertices[vertexOffset + 1].normal + bary.z * vertices[vertexOffset + 2].normal));

  vec3 baseColorFactor = materials[gl_InstanceID].baseColorFactor.xyz;
  vec3 emissionFactor = materials[gl_InstanceID].emissionFactor;
  float roughnessFactor = max(materials[gl_InstanceID].roughnessFactor, 0.001f);
  float metallicFactor = materials[gl_InstanceID].metallicFactor;
  float transmissionFactor = materials[gl_InstanceID].transmissionFactor;
  float ior = materials[gl_InstanceID].ior;

  // Determine interaction type based on the emission, roughness and metallic and transmission factor.
  uint interaction = determineMicrofacetInteractionType(roughnessFactor, metallicFactor, transmissionFactor);

  // Apple emission.
  payload.accColor += payload.mask * emissionFactor * 10.0f;

  // Compute orthonormal basis.
  vec3 ffNormal = (dot(normal, gl_WorldRayDirectionNV) < 0.0f) ? normal : normal * -1.0f;
  vec3 u = normalize(cross((abs(ffNormal.x) > 0.1f ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f)), ffNormal));
  vec3 v = cross(ffNormal, u);

  vec3 viewDir;
  vec3 lightDir;
  viewDir.x = dot(-gl_WorldRayDirectionNV, u);
  viewDir.y = dot(-gl_WorldRayDirectionNV, v);
  viewDir.z = dot(-gl_WorldRayDirectionNV, ffNormal);

  if (interaction == kDiff) {
    payload.mask *= DiffuseBSDF(baseColorFactor.xyz, viewDir, roughnessFactor, lightDir);
  } else if (interaction == kMetallic) {
    payload.mask *= ConductorBRDF(baseColorFactor.xyz, viewDir, roughnessFactor, lightDir);
  } else if (interaction == kTrans) {
    bool outside = dot(normal, -gl_WorldRayDirectionNV) > 0.0f;
    payload.mask *= DielectricBSDF(baseColorFactor.xyz, viewDir, roughnessFactor, transmissionFactor, ior, lightDir, outside);
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
    traceNV(accelerator, gl_RayFlagsOpaqueNV, 0xff, 0, 0, 0, gl_WorldRayOriginNV + gl_HitTNV * gl_WorldRayDirectionNV + ffNormal * EPS, 0.001, normalize(lightDir), 10000.0, 0);
  }
}