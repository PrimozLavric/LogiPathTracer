//
// Created by primoz on 26. 08. 19.
//

#ifndef LOGIPATHTRACER_RTX_UNIFORMS_H
#define LOGIPATHTRACER_RTX_UNIFORMS_H

#include "../common/ray.glsl"

struct Camera {
  mat4 worldMatrix;
  float fovY;
};

struct Material {
  vec4 baseColorFactor;
  vec3 emissionFactor;
  float metallicFactor;
  float roughnessFactor;
  float transmissionFactor;
  float ior;
  uint verticesOffset;
};

struct Vertex {
  vec3 normal;
};

struct RayPayload {
  vec3 mask;
  vec3 accColor;
  uint depth;
  uvec2 seed;
};

layout(set = 0, binding = 0, rgba32f) uniform image2D accumulationImage;

layout(std140, set = 0, binding = 1) uniform UBO {
  Camera camera;
  uint sampleCount;
}
ubo;

layout(set = 0, binding = 2) uniform accelerationStructureNV accelerator;

layout(std430, set = 0, binding = 3) buffer MaterialsBuffer {
  Material materials[];
};

layout(std430, set = 0, binding = 4) buffer VertexBuffer {
  Vertex vertices[];
};

#endif// LOGIPATHTRACER_RTX_UNIFORMS_H
