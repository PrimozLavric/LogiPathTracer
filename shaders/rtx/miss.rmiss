#version 460
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "uniforms.h"

layout(location = 0) rayPayloadInNV RayPayload payload;

void main() {
    payload.mask = vec3(0.0, 0.0, 1.0f);
}
