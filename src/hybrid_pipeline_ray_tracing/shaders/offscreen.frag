/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "../../framework/shaders/common.glsl"
#include "../../framework/shaders/constants.h"
#include "../../framework/shaders/definitions.glsl"
#include "./hybrid_constants.h"

layout(binding = 0, set = 1) uniform sampler2D textures[];
layout(binding = 1, set = 1) buffer _Materials { MaterialProperties m[]; }
materials;

layout(binding = 0, set = 2) buffer _Lights { LightProperties l[]; }
lighting;

layout(binding = 0) uniform SceneProperties
{
    mat4 projection;
    mat4 model;
    mat4 view;
    mat4 viewInverse;
    mat4 projInverse;
    vec4 overrideSunDirection;
}
scene;

layout(push_constant) uniform Index { int i; }
materialIndex;

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inBitangent;
layout(location = 4) in vec3 inEyePos;

layout(location = 0) out vec4 outFragColor;
layout(location = 1) out vec4 outFragNormals;
layout(location = 2) out vec4 outFragReflectRefractMap;

void main()
{
    vec3 N = inNormal;
    vec3 B = inBitangent;
    vec3 T = inTangent;
    mat3 TBN = mat3(T, B, N);
    vec3 eyeVector = normalize(-inEyePos);

    MaterialProperties material = materials.m[materialIndex.i];
    const int diffuseMapIndex = material.diffuseMapIndex;
    const int normalMapIndex = material.normalMapIndex;
    const int emissiveMapIndex = material.emissiveMapIndex;

    vec3 shadingNormal;
    if (normalMapIndex >= 0) {
        shadingNormal = texture(textures[nonuniformEXT(normalMapIndex)], inUV).rgb;
        shadingNormal = normalize(shadingNormal * 2.0 - 1.0);
        shadingNormal = TBN * shadingNormal;
    } else {
        shadingNormal = inNormal;
    }

    // ####  Compute surface albedo ####
    vec4 surfaceAlbedo;
    if (diffuseMapIndex >= 0) {
        surfaceAlbedo = texture(textures[nonuniformEXT(diffuseMapIndex)], inUV).rgba;
    } else {
        surfaceAlbedo = vec4(material.diffuse.rgb, material.opacity);
    }
    // ####  End compute surface albedo ####

    // ####  Compute surface emission ####
    vec3 surfaceEmissive;
    if (material.emissiveMapIndex >= 0) {
        surfaceEmissive = texture(textures[nonuniformEXT(material.emissiveMapIndex)], inUV).rgb;
    } else {
        surfaceEmissive = material.emissive.rgb;
    }
    // ####  End compute surface albedo ####

    // ####  Compute direct ligthing ####
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);
    vec3 emissive = surfaceEmissive;
    const float ambient = AMBIENT_WEIGHT;

    for (int i = 0; i < lighting.l.length(); ++i) {
        const LightProperties light = lighting.l[i];
        vec3 lightDir = vec3(0.0f);
        vec3 lightIntensity = vec3(0.0f);

        if (light.lightType == 1) { // Directional light (SUN)
            lightDir = (transpose(mat3(scene.viewInverse)) * light.direction.xyz).xyz
                + vec3(scene.overrideSunDirection);
            lightIntensity = light.diffuse.rgb * SUN_POWER;
            lightDir = normalize(lightDir);
        } else {
            continue;
        }

        const float cosThetaLight = dot(shadingNormal, -lightDir);
        diffuse += lightIntensity * max(cosThetaLight, ambient);
        if (material.shininessStrength > 0) {
            const vec3 r = reflect(lightDir, shadingNormal);
            specular += lightIntensity * pow(max(0, dot(r, eyeVector)), material.shininessStrength);
        }
    }
    const float reflectivity = material.reflectivity;
    const float endRefractIdx = material.refractIdx;
    float startRefractIdx = 1.0f;
    float reflectPercent = 0.0f;
    float refractPercent = 0.0f;
    if (endRefractIdx != NOT_REFRACTIVE_IDX) {
        reflectPercent = fresnel(eyeVector, shadingNormal, startRefractIdx, endRefractIdx);
        refractPercent = 1.0f - reflectPercent - surfaceAlbedo.a;
    } else if (reflectivity != NOT_REFLECTVE_IDX) {
        reflectPercent = reflectivity;
    }
    outFragNormals = vec4(vec3(transpose(scene.view) * vec4(shadingNormal, 1.0f)).xyz,
        surfaceAlbedo.a); // transform them back to view space
    outFragReflectRefractMap = vec4(reflectPercent, refractPercent, endRefractIdx, surfaceAlbedo.a);
    outFragColor
        = vec4(emissive + (diffuse + specular + ambient) * surfaceAlbedo.rgb, surfaceAlbedo.a);
}