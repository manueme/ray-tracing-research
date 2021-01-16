/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_VULKAN_MATERIAL_H
#define MANUEME_VULKAN_MATERIAL_H

#include <glm/glm.hpp>

struct ShaderMaterial {
    glm::vec3 ambient;
    glm::float32 pad0;
    glm::vec3 diffuse;
    glm::float32 pad1;
    glm::vec3 specular;
    glm::float32 pad2;
    glm::vec3 emissive;
    glm::float32 pad3;
    glm::int32 diffuseMapIndex = -1;
    glm::int32 normalMapIndex = -1;
    glm::int32 specularMapIndex = -1;
    glm::int32 emissiveMapIndex = -1;
    glm::float32 opacity = 1.f;
    glm::float32 reflectivity = -1.f; // NOT_REFLECTVE_IDX
    glm::float32 refractIdx = -1.f; // NOT_REFRACTIVE_IDX
    glm::float32 shininessStrength = 0.f;
};

#endif // MANUEME_VULKAN_MATERIAL_H
