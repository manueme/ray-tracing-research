/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "../core/texture.h"
#include "scene.h"
#include <assimp/pbrmaterial.h>

Material::Material() = default;

Material::Material(const ShaderMaterial& t_material)
    : m_shaderMaterial(t_material)
{
}

Material::Material(Device* t_device, VkQueue t_copyQueue, Scene* t_parent, const aiScene* t_scene,
    aiMaterial* t_aiMaterial)
{
    ShaderMaterial material;

    aiString aiName;
    t_aiMaterial->Get(AI_MATKEY_NAME, aiName);
    const auto name = std::string(aiName.C_Str());

    // Properties
    aiColor4D color;
    if (AI_SUCCESS == t_aiMaterial->Get(AI_MATKEY_COLOR_AMBIENT, color)) {
        material.ambient = glm::vec4(color.r, color.g, color.b, color.a);
    } else {
        material.ambient = glm::vec4(0.0f);
    }
    if (AI_SUCCESS == t_aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
        material.diffuse = glm::vec4(color.r, color.g, color.b, color.a);
    } else {
        material.diffuse = glm::vec4(0.0f);
    }
    if (AI_SUCCESS == t_aiMaterial->Get(AI_MATKEY_COLOR_SPECULAR, color)) {
        material.specular = glm::vec4(color.r, color.g, color.b, color.a);
    } else {
        material.specular = glm::vec4(0.0f);
    }
    if (AI_SUCCESS == t_aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, color)) {
        material.emissive = glm::vec4(color.r, color.g, color.b, color.a);
    } else {
        material.emissive = glm::vec4(0.0f);
    }

    float value;
    if (AI_SUCCESS == t_aiMaterial->Get(AI_MATKEY_OPACITY, value)) {
        material.opacity = value;
    }
    if (AI_SUCCESS == t_aiMaterial->Get(AI_MATKEY_REFLECTIVITY, value)) {
        material.reflectivity = value;
    }
    // AI_MATKEY_REFRACTI is not supported by most formats
    if (AI_SUCCESS == t_aiMaterial->Get(AI_MATKEY_REFRACTI, value)) {
        material.refractIdx = value;
    } else if (name.rfind("water", 0) == 0) {
        material.refractIdx = 1.333f;
    } else if (name.rfind("glass", 0) == 0) {
        material.refractIdx = 1.517f;
    }
    // ----

    if (AI_SUCCESS == t_aiMaterial->Get(AI_MATKEY_SHININESS_STRENGTH, value)) {
        material.shininessStrength = value;
    }
    if (AI_SUCCESS == t_aiMaterial->Get(AI_MATKEY_SHININESS, value)) {
        material.shininess = value;
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
