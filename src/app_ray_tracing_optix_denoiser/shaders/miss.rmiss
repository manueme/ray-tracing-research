/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : require

#include "app_definitions.glsl"

#include "../../framework/shaders/utils.glsl"

layout(location = RT_PAYLOAD_LOCATION) rayPayloadInEXT RayPayload rayInPayload;

void main()
{
    vec3 color = sky_ray(-gl_WorldRayDirectionEXT);
    rayInPayload.surfaceRadiance = color;
    rayInPayload.surfaceEmissive = vec3(0.0f);
    rayInPayload.surfaceAttenuation = vec3(1.0f);
    rayInPayload.done = 1;
    rayInPayload.hitDistance = RAY_MAX_HIT;
    rayInPayload.rayType = RAY_TYPE_MISS;
    rayInPayload.surfaceNormal = vec3(0.0f);
    rayInPayload.nextRayOrigin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * RAY_MAX_HIT;
}