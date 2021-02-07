#ifndef TRACE_RAY_GLSL
#define TRACE_RAY_GLSL

#include "acc_structure.glsl"

#ifdef USE_INPUT_PAYLOAD
layout(location = RT_PAYLOAD_LOCATION) rayPayloadInEXT RayPayload rayPayload;
#else
layout(location = RT_PAYLOAD_LOCATION) rayPayloadEXT RayPayload rayPayload;
#endif

void trace_ray(vec3 origin, vec3 direction, float minDistance, float maxDistance)
{
    uint rayFlags = gl_RayFlagsNoneEXT;
    traceRayEXT(topLevelAS,
        rayFlags,
        AS_FLAG_EVERYTHING,
        SBT_HIT_GROUP /*sbtRecordOffset*/,
        0 /*sbtRecordStride*/,
        SBT_MISS_INDEX /*missIndex*/,
        origin,
        minDistance,
        direction,
        maxDistance,
        RT_PAYLOAD_LOCATION);
}

#endif // TRACE_RAY_GLSL