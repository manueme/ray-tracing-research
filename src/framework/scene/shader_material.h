/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_VULKAN_MATERIAL_H
#define MANUEME_VULKAN_MATERIAL_H

#include <glm/glm.hpp>

struct ShaderMaterial {
    glm::vec4 ambient;
    glm::vec4 diffuse;
    glm::vec4 specular;
    glm::vec4 emissive;
    glm::int32 diffuseMapIndex = -1;
    glm::int32 normalMapIndex = -1;
    glm::int32 emissiveMapIndex = -1;
    glm::float32 opacity = 1.0f;
    glm::float32 reflectivity = 0.0f; // NOT_REFLECTVE_IDX
    glm::float32 refractIdx = 0.0f; // NOT_REFRACTIVE_IDX
    glm::float32 shininessStrength = 0.0f;
    glm::float32 shininess = 0.0f;
};

#endif // MANUEME_VULKAN_MATERIAL_H
