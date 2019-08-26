#ifndef LOGIPATHTRACER_COMMON_RAY_GLSL
#define LOGIPATHTRACER_COMMON_RAY_GLSL

#include "constants.glsl"

struct Ray {
  vec3 origin;
  vec3 direction;
};

float rayAABBIntersect(Ray ray, vec3 minCorner, vec3 maxCorner) {
  vec3 invDir = 1.0 / ray.direction;
  vec3 near = (minCorner - ray.origin) * invDir;
  vec3 far = (maxCorner - ray.origin) * invDir;

  vec3 tmin = min(near, far);
  vec3 tmax = max(near, far);

  float t0 = max(max(tmin.x, tmin.y), tmin.z);
  float t1 = min(min(tmax.x, tmax.y), tmax.z);

  if (t0 > t1) {
    return INFINITY;
  }

  // If we are outside the box
  if (t0 > 0.0) {
    return t0;
  }

  // If we are inside the box.
  if (t1 > 0.0) {
    return t1;
  }

  return INFINITY;
}

bool rayAABBIntersectTest(Ray ray, vec3 minCorner, vec3 maxCorner, float distance) {
  vec3 invDir = 1.0 / ray.direction;
  vec3 near = (minCorner - ray.origin) * invDir;
  vec3 far = (maxCorner - ray.origin) * invDir;

  vec3 tmin = min(near, far);
  vec3 tmax = max(near, far);

  float t0 = max(max(tmin.x, tmin.y), tmin.z);
  float t1 = min(min(tmax.x, tmax.y), tmax.z);

  if (t0 > t1) {
    return false;
  }

  // If we are outside the box
  if (t0 > 0.0) {
    return t0 < distance;
  }

  // If we are inside the box.
  return t1 > 0.0;;
}

float rayTriangleIntersect(Ray ray, vec3 v0, vec3 v1, vec3 v2) {
  vec3 edge1 = v1 - v0;
  vec3 edge2 = v2 - v0;
  vec3 pvec = cross(ray.direction, edge2);
  float det = 1.0 / dot(edge1, pvec);

  vec3 tvec = ray.origin - v0;
  float u = dot(tvec, pvec) * det;
  if (u < 0.0 || u > 1.0) {
    return INFINITY;
  }

  vec3 qvec = cross(tvec, edge1);
  float v = dot(ray.direction, qvec) * det;
  if (v < 0.0 || u + v > 1.0) {
    return INFINITY;
  }

  return dot(edge2, qvec) * det;
}

  #endif// LOGIPATHTRACER_COMMON_RAY_GLSL