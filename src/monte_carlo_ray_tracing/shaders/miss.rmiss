/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : require

#include "../../framework/shaders/constants.h"
#include "../../framework/shaders/definitions.glsl"

// layout(binding = 0, set = 6) uniform sampler2D rgbNoise;
// layout(binding = 1, set = 6) uniform sampler2D volumeNoise;

layout(location = RT_PAYLOAD_BRDF) rayPayloadInEXT RayPayload rayPayloadBRDF;

vec3 skyRay(vec3 dir)
{
    float t = 0.5 * (dir.y + 1.0);
    return (1.0 - t) * vec3(1.0, 1.0, 1.0) + t * vec3(0.5, 0.7, 1.0);
}

void main()
{
    vec3 color = skyRay(-gl_WorldRayDirectionEXT);
    rayPayloadBRDF.surfaceRadiance = color;
    rayPayloadBRDF.surfaceEmissive = vec3(0.0f);
    rayPayloadBRDF.surfaceAttenuation = vec3(1.0f);
    rayPayloadBRDF.done = 1;
    rayPayloadBRDF.hitDistance = RAY_MAX_HIT;
    rayPayloadBRDF.rayType = RAY_TYPE_MISS;
}