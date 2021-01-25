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

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(location = RT_PAYLOAD_BRDF) rayPayloadInEXT RayPayload rayPayload;
layout(location = RT_PAYLOAD_SHADOW) rayPayloadEXT RayPayloadShadow rayPayloadShadow;

hitAttributeEXT vec3 attribs;

layout(binding = 0, set = 1) uniform _SceneProperties
{
    mat4 viewInverse;
    mat4 projInverse;
    vec4 overrideSunDirection;
    int frameIteration;
    int frame;
    bool frameChanged;
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
    rayPayload.surfaceAttenuation = surfaceAlbedo.rgb;
    // ####  End compute surface albedo ####

    // #### Compute next ray direction ####
    const float reflectivity = material.reflectivity;
    const float endRefractIdx = material.refractIdx;
    float startRefractIdx = 1.0f;
    float reflectPercent = 0.0f;
    float refractPercent = 0.0f;
    if (endRefractIdx != NOT_REFRACTIVE_IDX) {
        reflectPercent = fresnel(hitDirection, shadingNormal, startRefractIdx, endRefractIdx);
        refractPercent = 1.0 - reflectPercent - surfaceAlbedo.a;
    } else if (reflectivity != NOT_REFLECTVE_IDX) {
        reflectPercent = reflectivity;
    }
    const bool computeReflectRefract = reflectPercent + refractPercent > 0.0f;
    if (computeReflectRefract) {
        const float splittingIndex = rnd(rayPayload.seed);
        if (splittingIndex < reflectPercent) { // REFLECT RAY
            // Default Values, they should not be used since it's a reflection ray
            resetRayPayload();
            // ---
            rayPayload.nextRayDirection = reflect(hitDirection, shadingNormal);
            rayPayload.rayType = RAY_TYPE_REFLECTION;
            return;
        } else if (splittingIndex < refractPercent + reflectPercent) { // REFRACT RAY
            float ior = startRefractIdx / endRefractIdx;
            const vec3 refractionRayDirection = refract(hitDirection, shadingNormal, ior);
            if (!is_zero(refractionRayDirection)) {
                // Default Values, they should not be used since it's a refraction ray
                resetRayPayload();
                // ---
                rayPayload.nextRayDirection = refractionRayDirection;
                rayPayload.rayType = RAY_TYPE_REFRACTION;
                return;
            }
        }
    }
    // if didn't return reflection or refraction then sample cosine hemisphere
    rayPayload.rayType = RAY_TYPE_DIFFUSE;
    float z1 = rnd(rayPayload.seed);
    float z2 = rnd(rayPayload.seed);
    vec3 sampledVec;
    cosine_sample_hemisphere(z1, z2, sampledVec);
    rayPayload.nextRayDirection = TBN * sampledVec;
    // #### End compute next ray direction ####

    // ####  Compute surface emission ####
    vec3 surfaceEmissive;
    if (material.emissiveMapIndex >= 0) {
        surfaceEmissive = texture(textures[nonuniformEXT(material.emissiveMapIndex)], hitUV).rgb;
    } else {
        surfaceEmissive = material.emissive.rgb;
    }
    // ####  End compute surface albedo ####

    // ####  Compute direct ligthing ####
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);
    vec3 emissive = surfaceEmissive;

    for (int i = 0; i < lighting.l.length(); ++i) {
        const LightProperties light = lighting.l[i];
        vec3 lightDir = vec3(0.0f);
        vec3 lightIntensity = vec3(0.0f);
        float maxHitDistance = RAY_MAX_HIT;
        if (light.lightType == 1) { // Directional light (SUN)
            lightDir = normalize(light.direction.xyz + vec3(scene.overrideSunDirection));
            lightIntensity = light.diffuse.rgb * SUN_POWER;
        } else if (light.lightType == 5) { // Area light
            const MaterialProperties areaMaterial = materials.m[light.areaMaterialIdx];
            const uint lightRandomPrimitiveID
                = uint(floor(light.areaPrimitiveCount * rnd(rayPayload.seed)));
            z1 = rnd(rayPayload.seed);
            z2 = rnd(rayPayload.seed);
            const Surface areaSurface = get_surface_instance(light.areaInstanceId,
                lightRandomPrimitiveID,
                uniform_sample_triangle(z1, z2));
            vec3 lightPosition = get_surface_pos(areaSurface);
            vec3 lightNormal = normalize(get_surface_normal(areaSurface));
            lightDir = hitPoint - lightPosition;
            float distSqr = dot(lightDir, lightDir);
            float dist = sqrt(distSqr);
            lightDir = normalize(lightDir);
            // Estimate full area of the light by multiplying by the number of triangles, this will
            // probably cause some very bright pixels, the clamping at the end of the raygen loop
            // will solve that issue:
            float area = get_surface_area(areaSurface) * light.areaPrimitiveCount;
            // ---
            float cosThetaAreaLight = abs(dot(lightNormal, lightDir));
            if (area == 0.0f || cosThetaAreaLight == 0.0f) {
                continue;
            }
            float lightPDF = distSqr / (cosThetaAreaLight * area);
            if (areaMaterial.emissiveMapIndex >= 0) {
                vec2 lightUV = get_surface_uv(areaSurface);
                lightIntensity
                    = texture(textures[nonuniformEXT(areaMaterial.emissiveMapIndex)], lightUV).rgb;
            } else {
                lightIntensity = areaMaterial.emissive.rgb;
            }
            lightIntensity /= lightPDF;
            maxHitDistance = max(0.0f, dist - 0.001f);
        } else {
            continue;
        }

        const float visibility = 1.0 - trace_shadow_ray(hitPoint, -lightDir, maxHitDistance);
        if (visibility > 0) {
            const float cosThetaLight = abs(dot(shadingNormal, lightDir));
            diffuse += visibility * lightIntensity * cosThetaLight;
            if (material.shininessStrength > 0) {
                const vec3 r = reflect(lightDir, shadingNormal);
                specular += visibility * lightIntensity
                    * pow(max(0, dot(r, eyeVector)), material.shininessStrength);
            }
        }
    }
    rayPayload.surfaceEmissive = emissive;
    rayPayload.surfaceRadiance = (diffuse + specular) * surfaceAlbedo.rgb;
    // ####  End Compute direct ligthing ####

    // Russian roulette termination
    // Slow (good for interiors):
    const float betaTermination = length(surfaceAlbedo);
    // Faster (bad for interiors):
    // const float betaTermination = max(surfaceAlbedo.r, max(surfaceAlbedo.g, surfaceAlbedo.b));
    if (rnd(rayPayload.seed) > betaTermination) {
        rayPayload.done = 1;
    }
    // ---
}
