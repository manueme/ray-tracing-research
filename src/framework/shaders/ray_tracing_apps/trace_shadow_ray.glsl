#ifndef TRACE_SHADOW_RAY_GLSL
#define TRACE_SHADOW_RAY_GLSL

#include "acc_structure.glsl"

layout(location = RT_PAYLOAD_SHADOW_LOCATION) rayPayloadEXT RayPayloadShadow rayPayloadShadow;

float trace_shadow_ray(vec3 origin, vec3 direction, float minDistance, float maxDistance)
{
    const uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT;
    rayPayloadShadow.shadowAmount = 0.0f;
    traceRayEXT(topLevelAS,
        rayFlags,
        AS_FLAG_EVERYTHING,
        SBT_SHADOW_HIT_GROUP /*sbtRecordOffset*/,
        0 /*sbtRecordStride*/,
        SBT_SHADOW_MISS_INDEX /*missIndex*/,
        origin,
        minDistance,
        direction,
        maxDistance,
        RT_PAYLOAD_SHADOW_LOCATION);
    return rayPayloadShadow.shadowAmount;
}

#endif // TRACE_SHADOW_RAY_GLSL