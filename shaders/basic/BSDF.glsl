#include "../common/random.glsl"

vec3 BasicDiffuseBRDF(vec3 F0, vec3 viewDir, out vec3 lightDir) {
  float r1 = 2.0f * PI * rand();// pick random number on unit circle (radius = 1, circumference = 2*Pi) for azimuth
  float r2 = rand();// pick random number for elevation
  float r2s = sqrt(r2);

  lightDir = vec3(cos(r1) * r2s, sin(r1) * r2s, sqrt(1.0f - r2));

  return F0 * dot(lightDir, vec3(0.0, 0.0, 1.0));
}

vec3 BasicSpecularBRDF(vec3 F0, vec3 viewDir, out vec3 lightDir) {
  lightDir = reflect(-viewDir, vec3(0.0, 0.0, 1.0));

  return F0;
}

float basicFresnelReflectance(vec3 n, vec3 nl, vec3 rayDirection, float nc, float nt, out vec3 tdir) {
  float nnt = dot(rayDirection, n) < 0.0 ? (nc / nt) : (nt / nc);

  tdir = refract(rayDirection, nl, nnt);

  // Original Fresnel equations
  float cosThetaInc = dot(nl, rayDirection);
  float cosThetaTra = dot(nl, tdir);
  float coefPara = (nt * cosThetaInc - nc * cosThetaTra) / (nt * cosThetaInc + nc * cosThetaTra);
  float coefPerp = (nc * cosThetaInc - nt * cosThetaTra) / (nc * cosThetaInc + nt * cosThetaTra);

  return (coefPara * coefPara + coefPerp * coefPerp) * 0.5;// Unpolarized
}

vec3 BasicTransmittanceBRDF(vec3 F0, vec3 viewDir, float transmittance, float ior, bool outside, out vec3 lightDir) {
  float nc = 1.0f;
  float nt = 1.5f;
  vec3 tdir;

  vec3 normal = outside ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 0.0, -1.0);
  float Re = basicFresnelReflectance(normal, vec3(0.0, 0.0, 1.0), -viewDir, 1.0, ior, tdir);

  float r = rand();

  if (r < Re) { // reflect ray from surface
    lightDir = reflect(-viewDir, normal);
    return vec3(1.0);
  } else { // transmit ray through surface
    lightDir = tdir;
    return F0 * transmittance;
  }
}