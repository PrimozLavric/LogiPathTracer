#ifndef LOGIPATHTRACER_COMMON_UTIL_GLSL
#define LOGIPATHTRACER_COMMON_UTIL_GLSL

float SRGBToLinear(float srgb) {
  if (srgb <= 0.04045)
    return srgb / 12.92;
  return pow((srgb + 0.055) / 1.055, 2.4);
}

vec3 SRGBToLinear(vec3 v) {
  return vec3(SRGBToLinear(v.x), SRGBToLinear(v.y), SRGBToLinear(v.z));
}

vec4 SRGBToLinear(vec4 v) {
  return vec4(SRGBToLinear(v.x), SRGBToLinear(v.y), SRGBToLinear(v.z), SRGBToLinear(v.w));
}

float Pow5(float x) {
  float x2 = x * x;
  return x2 * x2 * x;
}

vec3 barycentricCoord(vec3 point, vec3 v0, vec3 v1, vec3 v2) {
  vec3 ab = v1 - v0;
  vec3 ac = v2 - v0;
  vec3 ah = point - v0;

  float ab_ab = dot(ab, ab);
  float ab_ac = dot(ab, ac);
  float ac_ac = dot(ac, ac);
  float ab_ah = dot(ab, ah);
  float ac_ah = dot(ac, ah);

  float inv_denom = 1.0 / (ab_ab * ac_ac - ab_ac * ab_ac);

  float v = (ac_ac * ab_ah - ab_ac * ac_ah) * inv_denom;
  float w = (ab_ab * ac_ah - ab_ac * ab_ah) * inv_denom;
  float u = 1.0 - v - w;

  return vec3(u, v, w);
}

#endif // LOGIPATHTRACER_COMMON_UTIL_GLSL