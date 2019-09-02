#ifndef LOGIPATHTRACER_SHADERS_HEITZ_BSDF_GLSL
#define LOGIPATHTRACER_SHADERS_HEITZ_BSDF_GLSL

#include "../common/constants.glsl"
#include "../common/util.glsl"
#include "../common/random.glsl"

#define HEITZ_MAX_ORDER 3

float Fresnel(const float vdoth, const float eta) {
    const float cos_theta_t2 = 1.0f - (1.0f - vdoth * vdoth) / (eta * eta);

    // total internal reflection
    if (cos_theta_t2 <= 0.0f)
    return 1.0f;

    const float cos_theta_t = sqrt(cos_theta_t2);

    const float Rs = (vdoth - eta * cos_theta_t) / (vdoth + eta * cos_theta_t);
    const float Rp = (eta * vdoth - cos_theta_t) / (eta * vdoth + cos_theta_t);

    const float F = 0.5f * (Rs * Rs + Rp * Rp);
    return F;
}

vec3 refractEta(vec3 wi, vec3 wm, float eta) {
    const float cos_theta_i = dot(wi, wm);
    const float cos_theta_t2 = 1.0f - (1.0f - cos_theta_i * cos_theta_i) / (eta * eta);
    const float cos_theta_t = -sqrt(max(0.0f, cos_theta_t2));

    return wm * (dot(wi, wm) / eta + cos_theta_t) - wi / eta;
}

vec3 SchlickFresnel(float vdoth, vec3 F0) {
    vdoth = max(vdoth, 0.0f);
    return F0 + (vec3(1.0f) - F0) * Pow5(1.0f - vdoth);
}

// Adapted from "Sampling the GGX Distribution of Visible Normals",
// http://jcgt.org/published/0007/04/01/
vec3 SampleGGXVNDF(vec3 Ve, float alpha) {
    float R1 = rand();
    float R2 = rand();

    // Transforming the view direction to the hemisphere configuration
    vec3 Vh = normalize(vec3(alpha * Ve.x, alpha * Ve.y, Ve.z));

    // Orthonormal basis
    vec3 T1 = (Vh.z < 1.0f) ? normalize(cross(vec3(0.0f, 0.0f, 1.0f), Vh)) : vec3(1.0f, 0.0f, 0.0f);
    vec3 T2 = cross(Vh, T1);

    // Parameterization of the projected area
    float r = sqrt(R1);
    float phi = 2.0f * PI * R2;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5f * (1.0f + Vh.z);
    t2 = (1.0f - s) * sqrt(1.0f - t1 * t1) + s * t2;

    // Reprojection onto hemisphere
    vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0f, 1.0f - t1 * t1 - t2 * t2)) * Vh;

    // Transform the normal back to the ellipsoid configuration
    vec3 Ne = normalize(vec3(alpha * Nh.x, alpha * Nh.y, max(0.0f, Nh.z)));

    return Ne;
}

// Adapted from "Additional Progress Towards the Unification of Microfacet
// and Microflake Theories"
// http://onrendering.com/~jdupuy/
float SampleGGXHeight(vec3 dir, float height, float alpha) {
    float len = length(dir * vec3(alpha, alpha, 1));

    // Clamp projected area to a small positive value.
    // This avoids rare cases of 0/0 = nan, when U = 0 and
    // projectedArea = 0, without branching.
    float projectedArea = max(0.5f * (len - dir.z), 1e-7f);

    float R = rand();
    float delta = -log(1.0f - R) * dir.z / projectedArea;

    return height + delta;
}

// Adapted from "Multiple-Scattering Microfacet BSDFs with the Smith Model",
// https://eheitzresearch.wordpress.com/240-2/
vec3 SampleConductorPhaseFunction(vec3 F0, vec3 viewDir, float alpha, out vec3 weight) {
    // Generate micro normal according to the distribution of visible normals
    vec3 microNormal = SampleGGXVNDF(viewDir, alpha);

    // In some cases, the dot product can go slightly out of the expected
    // [0, 1] range, due to floating point imprecision
    float vdoth = dot(viewDir, microNormal);
    vdoth = clamp(vdoth, 0.0f, 1.0f);

    // Perfect mirror reflection
    vec3 reflDir = 2.0f * microNormal * vdoth - viewDir;

    // Schlick Fresnel factor
    weight = F0;//SchlickFresnel(vdoth, F0);

    return reflDir;
}

// Adapted from "Multiple-Scattering Microfacet BSDFs with the Smith Model",
// https://eheitzresearch.wordpress.com/240-2/
vec3 ConductorBRDF(vec3 F0, vec3 viewDir, float roughness, out vec3 lightDir) {
    float alpha = roughness * roughness;
    vec3 energy = vec3(1.0f);

    // Init
    lightDir = -viewDir;
    float height = 0.0f;

    // Random walk
    int order = 0;
    while (order < HEITZ_MAX_ORDER) {
        // Next height
        height = SampleGGXHeight(lightDir, height, alpha);

        // Left the microsurface?
        if (height > 0.0f) {
            break;
        }

        // Next direction, plus weight
        vec3 weight;
        lightDir = SampleConductorPhaseFunction(F0, -lightDir, alpha, weight);

        // Update energy throughput
        energy *= weight;

        order++;
    }

    return energy;
}

vec3 SampleDielectricPhaseFunction(vec3 viewDir, float alpha, float eta, inout bool outside) {
    // Generate micro normal according to the distribution of visible normals
    vec3 microNormal = SampleGGXVNDF(viewDir, alpha);

    float vdoth = dot(viewDir, microNormal);

    // Schlick Fresnel factor
    float F = Fresnel(vdoth, eta);

    if (rand() < F) {
        // Reflect
        vec3 reflDir = 2.0f * microNormal * vdoth - viewDir;
        return reflDir;
    } else {
        // Refract
        outside = !outside;
        return normalize(refractEta(viewDir, microNormal, eta));
    }
}

vec3 DielectricBSDF(vec3 F0, vec3 viewDir, float roughness, float transmittance, float ior, out vec3 lightDir,
bool outside) {
    float alpha = roughness * roughness;

    // Init
    lightDir = -viewDir;
    float height = 0.0f;

    // Random walk
    int order = 0;
    while (order < 1) {
        // Next height
        if (outside) {
            height = SampleGGXHeight(lightDir, height, alpha);

            // Left the microsurface?
            if (height > 0.0f) {
                break;
            }
        } else {
            height = -SampleGGXHeight(lightDir, -height, alpha);

            // Left the microsurface?
            if (height < 0.0f) {
                break;
            }
        }

        // Next direction, plus weight
        lightDir = SampleDielectricPhaseFunction(-lightDir, alpha, (outside ? ior : 1.0f / ior), outside);

        order++;
    }

    return F0;
}

vec3 SampleDiffusePhaseFunction(vec3 viewDir, float alpha) {
    // Generate micro normal according to the distribution of visible normals
    vec3 microNormal = SampleGGXVNDF(viewDir, alpha);

    vec3 u = (microNormal.z < 1.0f) ? normalize(cross(vec3(0.0f, 0.0f, 1.0f), microNormal)) : vec3(1.0f, 0.0f, 0.0f);
    vec3 v = cross(microNormal, u);

    float R1 = 2.0f * rand() - 1.0f;
    float R2 = 2.0f * rand() - 1.0f;

    float phi, R;

    if (R1 == 0 && R2 == 0) {
        R = phi = 0;
    } else if (R1 * R1 > R2 * R2) {
        R = R1;
        phi = (PI / 4.0f) * (R2 / R1);
    } else {
        R = R2;
        phi = (PI / 2.0f) - (R1 / R2) * (PI / 4.0f);
    }

    float x = R * cos(phi);
    float y = R * sin(phi);
    float z = sqrt(max(0.0f, 1.0f - x * x - y * y));
    vec3 lightDir = x * u + y * v + z * microNormal;

    return lightDir;
}

vec3 DiffuseBSDF(vec3 F0, vec3 viewDir, float roughness, out vec3 lightDir) {
    float alpha = roughness * roughness;
    vec3 energy = vec3(1.0f);

    // Init
    lightDir = -viewDir;
    float height = 0.0f;

    // Random walk
    int order = 0;
    while (order < HEITZ_MAX_ORDER) {
        // Next height
        height = SampleGGXHeight(lightDir, height, alpha);

        // Left the microsurface?
        if (height > 0.0f) {
            break;
        }

        // Next direction, plus weight
        lightDir = SampleDiffusePhaseFunction(-lightDir, alpha);

        // Update energy throughput
        energy *= F0;

        order++;
    }

    if (order >= HEITZ_MAX_ORDER) {
        lightDir = vec3(0.0f, 0.0f, 1.0f);
        return vec3(0.0f);
    }

    return energy;
}

    #endif// LOGIPATHTRACER_SHADERS_HEITZ_BSDF_GLSL
