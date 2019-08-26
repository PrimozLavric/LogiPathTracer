#ifndef LOGIPATHTRACER_COMMON_RANDOM_GLSL
#define LOGIPATHTRACER_COMMON_RANDOM_GLSL

#ifndef RANDOM_SEED_SET
#define RANDOM_SEED_SET
uvec2 seed;
#endif

float rand() {
  seed += uvec2(1);
  // return fract(sin(dot(seed.xy, vec2(12.9898, 78.233))) * 43758.5453);
  uvec2 q = 1103515245U * ((seed >> 1U) ^ (seed.yx));
  uint n = 1103515245U * ((q.x) ^ (q.y >> 3U));
  return float(n) * (1.0 / float(0xffffffffU));
}

  #endif// LOGIPATHTRACER_COMMON_RANDOM_GLSL