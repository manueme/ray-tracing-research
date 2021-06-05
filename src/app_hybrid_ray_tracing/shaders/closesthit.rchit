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
#define USE_INPUT_PAYLOAD
#include "../../framework/shaders/ray_tracing_apps/trace_ray.glsl"
#undef USE_INPUT_PAYLOAD
#include "../../framework/shaders/ray_tracing_apps/trace_shadow_ray_utils.glsl"

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
    const vec3 hitPoint
        = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * (gl_HitTEXT - CAMERA_NEAR);
    vec3 hitNormal;
    vec3 hitTangent;
    vec3 hitBitangent;
    get_surface_tangent_space(hitSurface, hitNormal, hitTangent, hitBitangent);
    const mat3 TBN = mat3(hitTangent, hitBitangent, hitNormal);
    vec3 hitDirection = gl_WorldRayDirectionEXT;
    const vec3 eyeVector = normalize(-hitDirection);

    uint currentDepth = rayPayload.depth;
    int done = (currentDepth >= maxDepth - 1) ? 1 : 0;

    // ####  Compute surface normal ####
    vec3 shadingNormal = get_surface_normal(material, hitNormal, TBN, hitUV);
    bool inside = false;
    if (dot(hitDirection, shadingNormal) > 0) {
        shadingNormal = -shadingNormal;
        inside = true;
    };
    // #### End compute surface normal ####

    // ####  Compute surface albedo ####
    vec4 surfaceAlbedo = get_surface_albedo(material, hitUV);
    // ####  End compute surface albedo ####

    // #### Compute recursive reflections and refractions ####
    vec3 refractions = vec3(0.0f);
    vec3 reflections = vec3(0.0f);
    float reflectPercent = 0.0f;
    float refractPercent = 0.0f;
    float surfacePercent = 1.0f;
    if (done == 0) {
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
        if (refractPercent > 0.0f) { // REFRACT RAY
            const vec3 refractionRayDirection = refract(hitDirection, shadingNormal, ior);
            if (!is_zero(refractionRayDirection)) {
                rayPayload.surfaceRadiance = vec3(0.0);
                rayPayload.rayType = RAY_TYPE_REFRACTION;
                rayPayload.depth = currentDepth + 1;
                trace_ray(hitPoint, refractionRayDirection, 0.0f, CAMERA_FAR);
                refractions = rayPayload.surfaceRadiance;
            } else {
                // total internal reflection
                reflectPercent += refractPercent;
            }
        }
        if (reflectPercent > 0.0f) { // REFLECT RAY
            vec3 reflectDirection = reflect(hitDirection, shadingNormal);
            rayPayload.surfaceRadiance = vec3(0.0);
            rayPayload.rayType = RAY_TYPE_REFLECTION;
            rayPayload.depth = currentDepth + 1;
            trace_ray(hitPoint, reflectDirection, 0.0f, CAMERA_FAR);
            reflections = rayPayload.surfaceRadiance;
        }
    }
    // #### End compute recursive reflections and refractions ####

    // ####  Compute direct ligthing ####
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);
    vec3 ambient = vec3(AMBIENT_WEIGHT);
    for (int i = 0; i < lighting.l.length(); ++i) {
        const LightProperties light = lighting.l[i];
        vec3 lightDir = vec3(0.0f);
        vec3 lightIntensity = vec3(0.0f);
        if (light.lightType == 1) { // Directional light (SUN)
            lightIntensity
                = sample_sun_light(light, scene.overrideSunDirection.xyz, hitPoint, 0.0f, lightDir);
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
    vec3 surfaceRadiance = (max(diffuse, ambient) + specular) * surfaceAlbedo.rgb;
    // ####  End Compute direct ligthing ####

    rayPayload.surfaceRadiance = reflections * reflectPercent + refractions * refractPercent
        + surfaceRadiance * surfacePercent;

    rayPayload.rayType = RAY_TYPE_DIFFUSE;
    rayPayload.hitDistance = gl_HitTEXT;
    rayPayload.done = done;
}
