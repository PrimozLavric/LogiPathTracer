#version 450

#extension GL_ARB_separate_shader_objects : enable

layout (binding = 0) uniform sampler2D samplerColor;

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 color;

vec4 ToneMap(in vec4 c, float limit) {
  float luminance = 0.3*c.x + 0.6*c.y + 0.1*c.z;

  return c * 1.0/(1.0 + luminance/limit);
}

const float gamma = 2.2;
const float exposure = 1.5;

void main() {
  vec2 uv = vec2(inUV.x, 1.0 - inUV.y);
  vec3 hdrColor = texture(samplerColor, uv).rgb;

  // Exposure tone mapping
  vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);
  // Gamma correction
  mapped = pow(mapped, vec3(1.0 / gamma));

  color = vec4(mapped, 1.0);
  /*
  color = texture(samplerColor, vec2(inUV.s, 1.0 - inUV.t));
  color =  pow(ToneMap(color, 1.5), vec4(1.0 / gamma));*/
}