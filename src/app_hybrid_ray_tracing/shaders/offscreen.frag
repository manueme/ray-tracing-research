/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "app_definitions.glsl"
#undef SCENE_SET
#define SCENE_SET 0 // override SCENE_SET for this particular case
#include "../../framework/shaders/utils.glsl"
#include "app_scene.glsl"

layout(binding = 0, set = 1) uniform sampler2D textures[];
layout(binding = 1, set = 1) buffer _Materials { MaterialProperties m[]; }
materials;

layout(binding = 0, set = 2) buffer _Lights { LightProperties l[]; }
lighting;

layout(push_constant) uniform Index { int i; }
materialIndex;

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inBitangent;
layout(location = 4) in vec3 inEyePos;

layout(location = 0) out vec4 outFragMaterial;
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

    // ####  Compute surface albedo ####
    vec4 surfaceAlbedo;
    if (diffuseMapIndex >= 0) {
        surfaceAlbedo = texture(textures[nonuniformEXT(diffuseMapIndex)], inUV).rgba;
    } else {
        surfaceAlbedo = vec4(material.diffuse.rgb, material.opacity);
    }
    if (surfaceAlbedo.a < 0.5 && material.refractIdx == NOT_REFRACTIVE_IDX) {
        discard;
    }
    // ####  End compute surface albedo ####

    // #### Compute shading normal ####
    vec3 shadingNormal;
    if (normalMapIndex >= 0) {
        shadingNormal = texture(textures[nonuniformEXT(normalMapIndex)], inUV).rgb;
        shadingNormal = normalize(shadingNormal * 2.0 - 1.0);
        shadingNormal = TBN * shadingNormal;
    } else {
        shadingNormal = inNormal;
    }
    // #### End compute shading normal ####

    // #### Compute refraction and reflection maps ####
    const float reflectivity = material.reflectivity;
    const float endRefractIdx = material.refractIdx;
    float startRefractIdx = 1.0f;
    float ior = startRefractIdx / endRefractIdx;
    float reflectPercent = 0.0f;
    float refractPercent = 0.0f;
    float alphaWithoutRefractives = surfaceAlbedo.a;
    if (endRefractIdx != NOT_REFRACTIVE_IDX) {
        reflectPercent = fresnel(eyeVector, shadingNormal, startRefractIdx, endRefractIdx);
        refractPercent = 1.0f - reflectPercent - surfaceAlbedo.a;
        alphaWithoutRefractives = 1.0f;
    } else if (reflectivity != NOT_REFLECTVE_IDX) {
        reflectPercent = reflectivity * surfaceAlbedo.a;
    }
    // #### End compute refraction and reflection maps ####

    outFragNormals = vec4(vec3(transpose(scene.view) * vec4(shadingNormal, 1.0f)).xyz,
        surfaceAlbedo.a); // transform them back to view space
    outFragReflectRefractMap = vec4(reflectPercent, refractPercent, ior, alphaWithoutRefractives);
    outFragMaterial = vec4(inUV.x, inUV.y, float(materialIndex.i), 1.0);
}