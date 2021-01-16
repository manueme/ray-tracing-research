/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_MATERIAL_H
#define MANUEME_MATERIAL_H

#include "../core/device.h"
#include "shader_material.h"

class Scene;
class aiMaterial;
class aiScene;

class Material {
public:
    Material();
    explicit Material(const ShaderMaterial& t_material);
    Material(Device* t_device, VkQueue t_copyQueue, Scene* t_parent, const aiScene* t_scene,
        aiMaterial* t_aiMaterial);
    ShaderMaterial getShaderMaterial();
    bool isEmissive();

private:
    ShaderMaterial m_shaderMaterial;
};

#endif // MANUEME_MATERIAL_H
