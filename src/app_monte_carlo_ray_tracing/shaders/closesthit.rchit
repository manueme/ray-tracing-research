/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "app_definitions.glsl"
#include "app_scene.glsl"

#include "../../framework/shaders/ray_tracing_apps/lights.glsl"
#include "../../framework/shaders/ray_tracing_apps/trace_shadow_ray_utils.glsl"

layout(location = RT_PAYLOAD_LOCATION) rayPayloadInEXT RayPayload rayInPayload;

hitAttributeEXT vec3 attribs;

layout(push_constant) uniform Constants
{
    uint maxDepth;
    uint samples;
};

void main()
{
    const Surface hitSurface = get_surface_instance(gl_InstanceID, gl_PrimitiveID, attribs.xy);
    const uint materialIndex = instanceInfo.i[gl_InstanceID].materialIndex;
    const MaterialProperties material = materials.m[materialIndex];
    const vec2 hitUV = get_surface_uv(hitSurface);
    const vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 hitNormal;
    vec3 hitTangent;
    vec3 hitBitangent;
    get_surface_tangent_space(hitSurface, hitNormal, hitTangent, hitBitangent);
    const mat3 TBN = mat3(hitTangent, hitBitangent, hitNormal);
    vec3 hitDirection = gl_WorldRayDirectionEXT;
    const vec3 eyeVector = normalize(-hitDirection);

    // Store hitDistance
    rayInPayload.hitDistance = gl_HitTEXT;
    // ####

    // #### New Ray Origin ####
    rayInPayload.nextRayOrigin = hitPoint;
    // ####

    vec3 shadingNormal = get_surface_normal(material, hitNormal, TBN, hitUV);
    bool inside = false;
    if (dot(hitDirection, shadingNormal) > 0) {
        shadingNormal = -shadingNormal;
        inside = true;
    };
    // #### Store hitNormal ####
    rayInPayload.surfaceNormal = shadingNormal;
    // ####

    // ####  Compute surface albedo ####
    vec4 surfaceAlbedo = get_surface_albedo(material, hitUV);
    rayInPayload.surfaceAttenuation = surfaceAlbedo.rgb;
    // ####  End compute surface albedo ####

    // #### Compute next ray direction ####
    float reflectPercent;
    float refractPercent;
    float surfacePercent;
    float ior;
    get_reflect_refract_percent(material,
        hitDirection,
        surfaceAlbedo,
        shadingNormal,
        inside,
        reflectPercent,
        refractPercent,
        surfacePercent,
        ior);
    const float splittingIndex = rnd(rayInPayload.seed);
    if (splittingIndex < reflectPercent) { // REFLECT RAY
        // Reset payload
        rayInPayload.surfaceAttenuation = vec3(1.0f);
        rayInPayload.surfaceRadiance = vec3(0.0f);
        rayInPayload.surfaceEmissive = vec3(0.0f);
        // ---
        rayInPayload.nextRayDirection = reflect(hitDirection, shadingNormal);
        rayInPayload.rayType = RAY_TYPE_REFLECTION;
        return;
    }
    if (splittingIndex < refractPercent + reflectPercent) { // REFRACT RAY
        const vec3 refractionRayDirection = refract(hitDirection, shadingNormal, ior);
        // Reset payload
        rayInPayload.surfaceAttenuation = vec3(1.0f);
        rayInPayload.surfaceRadiance = vec3(0.0f);
        rayInPayload.surfaceEmissive = vec3(0.0f);
        // ---
        if (!is_zero(refractionRayDirection)) {
            rayInPayload.nextRayDirection = refractionRayDirection;
            rayInPayload.rayType = RAY_TYPE_REFRACTION;
            return;
        } else {
            // total internal reflection
            rayInPayload.nextRayDirection = reflect(hitDirection, shadingNormal);
            rayInPayload.rayType = RAY_TYPE_REFLECTION;
            return;
        }
    }
    // If didn't return reflection or refraction then sample cosine hemisphere
    rayInPayload.rayType = RAY_TYPE_DIFFUSE;
    float z1 = rnd(rayInPayload.seed);
    float z2 = rnd(rayInPayload.seed);
    vec3 sampledVec;
    cosine_sample_hemisphere(z1, z2, sampledVec);
    rayInPayload.nextRayDirection = TBN * sampledVec;
    // #### End compute next ray direction ####

    // ####  Compute surface emission ####
    vec3 surfaceEmissive = get_surface_emissive(material, hitUV).rgb;
    rayInPayload.surfaceEmissive = surfaceEmissive;
    // ####  End compute surface emission ####

    // ####  Compute direct ligthing ####
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);
    vec3 emissive = surfaceEmissive;
    for (int i = 0; i < lighting.l.length(); ++i) {
        const LightProperties light = lighting.l[i];
        vec3 lightDir = vec3(0.0f);
        vec3 lightIntensity = vec3(0.0f);
        if (light.lightType == 1) { // Directional light (SUN)
            lightIntensity = sample_sun_light(light,
                                 scene.overrideSunDirection.xyz,
                                 hitPoint,
                                 RAY_MIN_HIT,
                                 lightDir)
                * SUN_POWER;
        } else if (light.lightType == 5) { // Area light
            float n1 = rnd(rayInPayload.seed);
            float n2 = rnd(rayInPayload.seed);
            float n3 = rnd(rayInPayload.seed);
            lightIntensity = sample_area_light(light, hitPoint, RAY_MIN_HIT, lightDir, n1, n2, n3);
        } else {
            continue;
        }
        const float cosThetaLight = abs(dot(shadingNormal, lightDir));
        diffuse += lightIntensity * cosThetaLight;
        if (material.shininessStrength == 0) {
            continue;
        }
        const vec3 r = reflect(lightDir, shadingNormal);
        specular += lightIntensity * pow(max(0, dot(r, eyeVector)), material.shininess)
            * material.shininessStrength;
    }
    rayInPayload.surfaceEmissive = emissive;
    rayInPayload.surfaceRadiance = (diffuse + specular) * surfaceAlbedo.rgb;
    // ####  End Compute direct ligthing ####

    // Russian roulette termination
    // Slow (good for interiors):
    const float betaTermination = length(surfaceAlbedo);
    // Faster (bad for interiors):
    // const float betaTermination = max(surfaceAlbedo.r, max(surfaceAlbedo.g, surfaceAlbedo.b));
    if (rnd(rayInPayload.seed) > betaTermination) {
        rayInPayload.done = 1;
    }
    // ---
}
