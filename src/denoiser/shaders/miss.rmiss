/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : require

#include "../../framework/shaders/common.glsl"
#include "../../framework/shaders/constants.h"
#include "../../framework/shaders/definitions.glsl"

layout(location = RT_PAYLOAD_LOCATION) rayPayloadInEXT RayPayload rayPayloadBRDF;

void main()
{
    vec3 color = sky_ray(-gl_WorldRayDirectionEXT);
    rayPayloadBRDF.surfaceRadiance = color;
    rayPayloadBRDF.surfaceEmissive = vec3(0.0f);
    rayPayloadBRDF.surfaceAttenuation = vec3(1.0f);
    rayPayloadBRDF.done = 1;
    rayPayloadBRDF.hitDistance = RAY_MAX_HIT;
    rayPayloadBRDF.rayType = RAY_TYPE_MISS;
}