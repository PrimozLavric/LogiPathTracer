
#ifndef RANDOM_H
#define RANDOM_H

uvec2 seed;

float rand() {
  seed += uvec2(1);
  // return fract(sin(dot(seed.xy, vec2(12.9898, 78.233))) * 43758.5453);
  uvec2 q = 1103515245U * ((seed >> 1U) ^ (seed.yx));
  uint n = 1103515245U * ((q.x) ^ (q.y >> 3U));
  return float(n) * (1.0 / float(0xffffffffU));
}

#endif