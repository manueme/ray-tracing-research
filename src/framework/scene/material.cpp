/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "../core/texture.h"
#include "scene.h"
#include <vector>

Material::Material() = default;

Material::Material(const ShaderMaterial& t_material)
    : m_shaderMaterial(t_material)
{
}

Material::Material(Device* t_device, VkQueue t_copyQueue, Scene* t_parent,
    const aiScene* t_scene, aiMaterial* t_aiMaterial)
{
    ShaderMaterial material;

    aiString name;
    t_aiMaterial->Get(AI_MATKEY_NAME, name);

    // Properties
    aiColor4D color;
    if (AI_SUCCESS == t_aiMaterial->Get(AI_MATKEY_COLOR_AMBIENT, color)) {
        material.ambient = glm::vec3(color.r, color.g, color.b);
    } else {
        material.ambient = glm::vec3(0.0f);
    }
    if (AI_SUCCESS == t_aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
        material.diffuse = glm::vec3(color.r, color.g, color.b);
    } else {
        material.diffuse = glm::vec3(0.0f);
    }
    if (AI_SUCCESS == t_aiMaterial->Get(AI_MATKEY_COLOR_SPECULAR, color)) {
        material.specular = glm::vec3(color.r, color.g, color.b);
    } else {
        material.specular = glm::vec3(0.0f);
    }
    if (AI_SUCCESS == t_aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, color)) {
        material.emissive = glm::vec3(color.r, color.g, color.b);
    } else {
        material.emissive = glm::vec3(0.0f);
    }

    float value;
    if (AI_SUCCESS == t_aiMaterial->Get(AI_MATKEY_OPACITY, value)) {
        material.opacity = value;
    }
    if (AI_SUCCESS == t_aiMaterial->Get(AI_MATKEY_REFLECTIVITY, value)) {
        material.reflectivity = value;
    }
    // TODO: fix materials not sending refraction idx information
    if (AI_SUCCESS == t_aiMaterial->Get(AI_MATKEY_REFRACTI, value)) {
        material.refractIdx = value;
    }
    if (name == aiString("water")) {
        material.refractIdx = 1.333f;
    }
    if (AI_SUCCESS == t_aiMaterial->Get(AI_MATKEY_SHININESS_STRENGTH, value)) {
        material.shininessStrength = value;
    }

    if (t_aiMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
        aiString textureFile;
        t_aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &textureFile);
        if (auto texture = t_scene->GetEmbeddedTexture(textureFile.C_Str())) {
            // embedded texture
            std::cout << "  Diffuse Map: \"" << texture->mFilename.C_Str() << "\"" << std::endl;
            VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
            VulkanTexture2D texture2D;
            texture2D.loadFromAssimp(texture, format, t_device, t_copyQueue);
            material.diffuseMapIndex = static_cast<int>(t_parent->textures.size());
            t_parent->textures.push_back(texture2D);
        }
    }
    if (t_aiMaterial->GetTextureCount(aiTextureType_NORMALS) > 0) {
        aiString textureFile;
        t_aiMaterial->GetTexture(aiTextureType_NORMALS, 0, &textureFile);
        if (auto texture = t_scene->GetEmbeddedTexture(textureFile.C_Str())) {
            // embedded texture
            std::cout << "  Normal Map: \"" << texture->mFilename.C_Str() << "\"" << std::endl;
            VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
            VulkanTexture2D texture2D;
            texture2D.loadFromAssimp(texture, format, t_device, t_copyQueue);
            material.normalMapIndex = static_cast<int>(t_parent->textures.size());
            t_parent->textures.push_back(texture2D);
        }
    }
    if (t_aiMaterial->GetTextureCount(aiTextureType_SPECULAR) > 0) {
        aiString textureFile;
        t_aiMaterial->GetTexture(aiTextureType_SPECULAR, 0, &textureFile);
        if (auto texture = t_scene->GetEmbeddedTexture(textureFile.C_Str())) {
            // embedded texture
            std::cout << "  Specular Map: \"" << texture->mFilename.C_Str() << "\"" << std::endl;
            VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
            VulkanTexture2D texture2D;
            texture2D.loadFromAssimp(texture, format, t_device, t_copyQueue);
            material.specularMapIndex = static_cast<int>(t_parent->textures.size());
            t_parent->textures.push_back(texture2D);
        }
    }
    if (t_aiMaterial->GetTextureCount(aiTextureType_EMISSIVE) > 0) {
        aiString textureFile;
        t_aiMaterial->GetTexture(aiTextureType_EMISSIVE, 0, &textureFile);
        if (auto texture = t_scene->GetEmbeddedTexture(textureFile.C_Str())) {
            // embedded texture
            std::cout << "  Emission Map: \"" << texture->mFilename.C_Str() << "\"" << std::endl;
            VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
            VulkanTexture2D texture2D;
            texture2D.loadFromAssimp(texture, format, t_device, t_copyQueue);
            material.emissiveMapIndex = static_cast<int>(t_parent->textures.size());
            t_parent->textures.push_back(texture2D);
        }
    }
    this->m_shaderMaterial = material;
}

ShaderMaterial Material::getShaderMaterial() { return this->m_shaderMaterial; }

bool Material::isEmissive()
{
    const auto emissive = this->m_shaderMaterial.emissive;
    return (emissive.r + emissive.g + emissive.b) > 0;
}
