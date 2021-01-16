/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_SHADER_LIGHT_H
#define MANUEME_SHADER_LIGHT_H

#include <glm/glm.hpp>

struct ShaderLight {
    glm::vec3 position {}; // 1 2 3
    glm::float32 pad0 {}; // 4
    glm::vec3 direction {}; // 1 2 3
    glm::float32 pad1 {}; // 4
    glm::vec3 diffuse {}; // 1 2 3
    glm::float32 pad2 {}; // 4
    glm::vec3 specular {}; // 1 2 3
    glm::float32 pad3 {}; // 4
    glm::int32 lightType {}; // 1  // aiLightSourceType
    glm::uint32 areaInstanceId {}; // 2
    glm::uint32 areaPrimitiveCount {}; // 3
    glm::uint32 areaMaterialIdx {}; // 4
};

#endif // MANUEME_SHADER_LIGHT_H
