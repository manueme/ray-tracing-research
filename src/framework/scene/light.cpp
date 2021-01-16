/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "light.h"

Light::Light(const aiLight& t_aiLight)
{
    m_shaderLight.diffuse = glm::f32vec3(t_aiLight.mColorDiffuse.r,
        t_aiLight.mColorDiffuse.g,
        t_aiLight.mColorDiffuse.b);
    m_shaderLight.specular = glm::f32vec3(t_aiLight.mColorSpecular.r,
        t_aiLight.mColorSpecular.g,
        t_aiLight.mColorSpecular.b);
    m_shaderLight.areaInstanceId = 0;
    m_shaderLight.areaMaterialIdx = 0;
    m_shaderLight.areaPrimitiveCount = 0;
    m_shaderLight.direction
        = glm::f32vec3(t_aiLight.mDirection.x, -t_aiLight.mDirection.y, t_aiLight.mDirection.z);
    m_shaderLight.position
        = glm::f32vec3(t_aiLight.mPosition.x, -t_aiLight.mPosition.y, t_aiLight.mPosition.z);
    m_shaderLight.lightType = static_cast<glm::int32>(t_aiLight.mType);
}

Light::Light(unsigned int t_instanceId, unsigned int t_materialIdx, unsigned int t_primitiveCount)
{
    m_shaderLight.areaInstanceId = t_instanceId;
    m_shaderLight.areaMaterialIdx = t_materialIdx;
    m_shaderLight.areaPrimitiveCount = t_primitiveCount;
    m_shaderLight.direction = glm::vec3(0, 0, 0);
    m_shaderLight.position = glm::vec3(0, 0, 0);
    m_shaderLight.lightType = static_cast<glm::int32>(aiLightSource_AREA);
}

ShaderLight Light::getShaderLight() { return m_shaderLight; }
