#ifndef LOGIPATHTRACER_SHADERS_HEITZ_INTERACTION_TYPE_GLSL
#define LOGIPATHTRACER_SHADERS_HEITZ_INTERACTION_TYPE_GLSL

// Interaction type
const uint kDiff = 0x00000001u;
const uint kMetallic = 0x00000002u;
const uint kTrans = 0x00000004u;

uint determineMicrofacetInteractionType(float roughnessFactor, float metallicFactor, float transmissionFactor) {
  float metallicBRDF = metallicFactor;
  float transmissionBSDF = (1.0f - metallicFactor) * transmissionFactor;
  float dielectricBRDF = (1.0f - transmissionFactor) * (1.0f - metallicFactor);

  float norm = 1.0f / (metallicBRDF + transmissionBSDF + dielectricBRDF);
  metallicBRDF *= norm;
  transmissionBSDF *= norm;
  dielectricBRDF *= norm;

  float r = rand();

  if (r < metallicBRDF) {
    return kMetallic;
  } else if (r < metallicBRDF + transmissionBSDF) {
    return kTrans;
  } else {
    return kDiff;
  }
}

  #endif// LOGIPATHTRACER_SHADERS_HEITZ_INTERACTION_TYPE_GLSL
