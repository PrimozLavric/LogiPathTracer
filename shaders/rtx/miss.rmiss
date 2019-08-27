#version 460
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "uniforms.glsl"

layout(location = 0) rayPayloadInNV RayPayload payload;

void main() {
  payload.accColor = payload.mask * 0.2;
}
