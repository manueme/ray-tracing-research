
/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "../../framework/shaders/constants.h" // to get M_PIf

bool is_nan(vec3 c) { return isnan(c.x) || isnan(c.y) || isnan(c.z); }

// Tiny Encryption Algorithm for seed generation
// https://es.wikipedia.org/wiki/Tiny_Encryption_Algorithm
uint tea(uint val0, uint val1)
{
    uint v0 = val0;
    uint v1 = val1;
    uint s0 = 0;

    for (uint n = 0; n < 16; n++) {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }

    return v0;
}

// Linear congruential generator
// https://en.wikipedia.org/wiki/Linear_congruential_generator Generate random
// unsigned int in [0, 2^24)
uint lcg(inout uint prev)
{
    uint LCG_A = 1664525u;
    uint LCG_C = 1013904223u;
    prev = (LCG_A * prev + LCG_C);
    return prev & 0x00FFFFFF;
}

// Generate random float in [0, 1)
float rnd(inout uint prev) { return (float(lcg(prev)) / float(0x01000000)); }

uint rot_seed(uint seed, uint frame) { return seed ^ frame; }

// Source:
// http://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations.html
void concentric_sample_disk(const float u1, const float u2, out vec2 p)
{
    // Map uniform random numbers to $[-1,1]^2$
    vec2 uOffset = 2.0 * vec2(u1, u2) - vec2(1.0, 1.0);

    // Handle degeneracy at the origin
    if (uOffset.x == 0 && uOffset.y == 0) {
        p = vec2(0, 0);
    } else {
        // Apply concentric mapping to point
        float theta;
        float r;
        if (abs(uOffset.x) > abs(uOffset.y)) {
            r = uOffset.x;
            theta = M_PI_OVER4f * (uOffset.y / uOffset.x);
        } else {
            r = uOffset.y;
            theta = M_PI_OVER2f - M_PI_OVER4f * (uOffset.x / uOffset.y);
        }
        p = r * vec2(cos(theta), sin(theta));
    }
}

// Malley’s Method
void cosine_sample_hemisphere(const float u1, const float u2, out vec3 p)
{
    vec2 disk;
    concentric_sample_disk(u1, u2, disk);
    p = vec3(disk.x, disk.y, sqrt(max(0.0, 1.0 - dot(disk, disk))));
}

vec2 uniform_sample_triangle(const float u1, const float u2)
{
    float su0 = sqrt(u1);
    return vec2(1 - su0, u2 * su0);
}

// Asuming all dielectric maretials for now...
float fresnel(const vec3 incident, vec3 normal, float etaI, float etaT)
{
    float cosThetaI = clamp(-dot(incident, normal), -1.0, 1.0);
    // Potentially swap indices of refraction
    const bool entering = cosThetaI > 0.0;
    if (!entering) {
        // const float auxEta = etaT;
        // etaT = etaI;
        // etaI = auxEta;
        cosThetaI = abs(cosThetaI);
    }

    // Compute cosThetaT using Snell's law
    const float sinThetaI = sqrt(max(0.0, 1.0 - cosThetaI * cosThetaI));
    const float sinThetaT = etaI / etaT * sinThetaI;

    // Handle total internal reflection
    if (sinThetaT >= 1) {
        return 1.0;
    }
    const float cosThetaT = sqrt(max(0.0, 1.0 - sinThetaT * sinThetaT));
    const float rParl
        = ((etaT * cosThetaI) - (etaI * cosThetaT)) / ((etaT * cosThetaI) + (etaI * cosThetaT));
    const float rPerp
        = ((etaI * cosThetaI) - (etaT * cosThetaT)) / ((etaI * cosThetaI) + (etaT * cosThetaT));
    return (rParl * rParl + rPerp * rPerp) / 2;
}
