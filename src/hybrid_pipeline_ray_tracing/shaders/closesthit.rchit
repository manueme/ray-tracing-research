/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "../../framework/shaders/common.glsl"
#include "../../framework/shaders/constants.h"
#include "../../framework/shaders/definitions.glsl"
#include "../../framework/shaders/vertex.glsl"
#include "./hybrid_constants.h"

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(location = RT_PAYLOAD_BRDF) rayPayloadInEXT RayPayload rayPayload;
layout(location = RT_PAYLOAD_SHADOW) rayPayloadEXT RayPayloadShadow rayPayloadShadow;

hitAttributeEXT vec3 attribs;

layout(binding = 0, set = 1) uniform _SceneProperties
{
    mat4 projection;
    mat4 model;
    mat4 view;
    mat4 viewInverse;
    mat4 projInverse;
    vec4 overrideSunDirection;
    int frame;
}
scene;
layout(binding = 0, set = 3) uniform sampler2D textures[];
layout(binding = 1, set = 3) buffer _Materials { MaterialProperties m[]; }
materials;
layout(binding = 0, set = 4) buffer _Lights { LightProperties l[]; }
lighting;

layout(push_constant) uniform Constants
{
    int maxDepth;
    int samples;
};

float trace_shadow_ray(vec3 origin, vec3 direction, float maxDistance)
{
    const uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT;
    rayPayloadShadow.shadowAmount = 0.0f;
    traceRayEXT(topLevelAS,
        rayFlags,
        AS_FLAG_EVERYTHING,
        SBT_MC_SHADOW_HIT_GROUP /*sbtRecordOffset*/,
        0 /*sbtRecordStride*/,
        SBT_MC_SHADOW_MISS_INDEX /*missIndex*/,
        origin,
        RAY_MIN_HIT,
        direction,
        maxDistance,
        RT_PAYLOAD_SHADOW);
    return rayPayloadShadow.shadowAmount;
}

void resetRayPayload()
{
    rayPayload.surfaceAttenuation = vec3(1.0f);
    rayPayload.surfaceRadiance = vec3(0.0f);
    rayPayload.surfaceEmissive = vec3(0.0f);
}

void main()
{
    const Surface hitSurface = get_surface_instance(gl_InstanceID, gl_PrimitiveID, attribs.xy);
    const uint materialIndex = instanceInfo.i[gl_InstanceID].materialIndex;
    const MaterialProperties material = materials.m[materialIndex];
    const int diffuseMapIndex = material.diffuseMapIndex;
    const vec2 hitUV = get_surface_uv(hitSurface);
    const vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    const vec3 hitNormal = get_surface_normal(hitSurface);
    vec3 hitTangent = get_surface_tangent(hitSurface);
    // re-orthogonalize T with respect to N
    hitTangent = normalize(hitTangent - dot(hitTangent, hitNormal) * hitNormal);
    const vec3 hitBitangent = normalize(cross(hitNormal, hitTangent));
    const mat3 TBN = mat3(hitTangent, hitBitangent, hitNormal);
    vec3 hitDirection = gl_WorldRayDirectionEXT;
    const vec3 eyeVector = normalize(-hitDirection);

    // Store hitDistance
    rayPayload.hitDistance = gl_HitTEXT;
    // ####

    // #### New Ray Origin ####
    rayPayload.nextRayOrigin = hitPoint;
    // ####

    const int normalMapIndex = material.normalMapIndex;
    vec3 shadingNormal;
    if (normalMapIndex >= 0) {
        shadingNormal = texture(textures[nonuniformEXT(normalMapIndex)], hitUV).rgb;
        shadingNormal = normalize(shadingNormal * 2.0 - 1.0);
        shadingNormal = TBN * shadingNormal;
    } else {
        shadingNormal = hitNormal;
    }
    bool inside = false;
    if (dot(hitDirection, shadingNormal) > 0) {
        shadingNormal = -shadingNormal;
        inside = true;
    };

    // ####  Compute surface albedo ####
    vec4 surfaceAlbedo;
    if (diffuseMapIndex >= 0) {
        surfaceAlbedo = texture(textures[nonuniformEXT(diffuseMapIndex)], hitUV);
    } else {
        surfaceAlbedo = vec4(material.diffuse.rgb, material.opacity);
    }
    // ####  End compute surface albedo ####

    rayPayload.rayType = RAY_TYPE_DIFFUSE;

    // ####  Compute direct ligthing ####
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);
    float ambient = AMBIENT_WEIGHT;
    for (int i = 0; i < lighting.l.length(); ++i) {
        const LightProperties light = lighting.l[i];
        vec3 lightDir = vec3(0.0f);
        vec3 lightIntensity = vec3(0.0f);
        float maxHitDistance = RAY_MAX_HIT;
        if (light.lightType == 1) { // Directional light (SUN)
            lightDir = normalize(light.direction.xyz + vec3(scene.overrideSunDirection));
            lightIntensity = light.diffuse.rgb;
        } else {
            continue;
        }
        const float visibility = 1.0 - trace_shadow_ray(hitPoint, -lightDir, maxHitDistance);
        const float cosThetaLight = abs(dot(shadingNormal, lightDir));
        diffuse += lightIntensity * max(visibility * cosThetaLight, ambient);
        if (material.shininessStrength == 0) {
            continue;
        }
        const vec3 r = reflect(lightDir, shadingNormal);
        specular += visibility * lightIntensity * pow(max(0, dot(r, eyeVector)), material.shininess)
            * material.shininessStrength;
    }
    rayPayload.surfaceRadiance = (diffuse + specular) * surfaceAlbedo.rgb;
    // ####  End Compute direct ligthing ####
}
