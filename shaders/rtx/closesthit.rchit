#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "uniforms.h"

layout(location = 0) rayPayloadInNV RayPayload payload;
hitAttributeNV vec3 attribs;

void main() {
  const vec3 bary = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
  //vec3 normal = vertices[materials[gl_InstanceID].verticesOffset + gl_PrimitiveID].normal;
  vec3 normal = normalize(mat3(gl_ObjectToWorldNV) * (bary.x * vertices[materials[gl_InstanceID].verticesOffset + gl_PrimitiveID * 3].normal + bary.y * vertices[materials[gl_InstanceID].verticesOffset + gl_PrimitiveID * 3 + 1].normal + bary.z * vertices[materials[gl_InstanceID].verticesOffset + gl_PrimitiveID * 3 + 2].normal));

  payload.mask = normal;
}