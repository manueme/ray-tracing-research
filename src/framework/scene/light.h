/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_LIGHT_H
#define MANUEME_LIGHT_H

#include "shader_light.h"
#include "shader_material.h"
#include <assimp/light.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

class Light {
public:
    Light(const aiLight& t_aiLight);
    Light(unsigned int t_instanceId, unsigned int t_materialIdx, unsigned int t_primitiveCount);
    ShaderLight getShaderLight();

private:
    ShaderLight m_shaderLight;
};

#endif // MANUEME_LIGHT_H
