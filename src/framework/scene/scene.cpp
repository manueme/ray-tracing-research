/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "scene.h"

uint32_t SceneVertexLayout::stride()
{
    uint32_t res = 0;
    for (auto& component : components) {
        switch (component) {
        case VERTEX_COMPONENT_UV:
            res += 2 * sizeof(float);
            break;
        case VERTEX_COMPONENT_DUMMY_FLOAT:
            res += sizeof(float);
            break;
        case VERTEX_COMPONENT_DUMMY_VEC4:
            res += 4 * sizeof(float);
            break;
        default:
            // All components except the ones listed above are made up of 3 floats
            res += 3 * sizeof(float);
        }
    }
    return res;
}

SceneCreateInfo::SceneCreateInfo()
    : center(glm::vec3(0.0f))
    , scale(glm::vec3(1.0f))
    , uvScale(glm::vec2(1.0f))
{
}

SceneCreateInfo::~SceneCreateInfo() = default;

SceneCreateInfo::SceneCreateInfo(glm::vec3 t_scale, glm::vec2 t_uvScale, glm::vec3 t_center)
{
    this->center = t_center;
    this->scale = t_scale;
    this->uvScale = t_uvScale;
}

SceneCreateInfo::SceneCreateInfo(float t_scale, float t_uvScale, float t_center)
{
    this->center = glm::vec3(t_center);
    this->scale = glm::vec3(t_scale);
    this->uvScale = glm::vec2(t_uvScale);
}

Scene::Scene() = default;

Scene::~Scene() = default;

void Scene::destroy()
{
    assert(m_device);
    vkDestroyBuffer(m_device->logicalDevice, vertices.buffer, nullptr);
    vkFreeMemory(m_device->logicalDevice, vertices.memory, nullptr);
    if (indices.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device->logicalDevice, indices.buffer, nullptr);
        vkFreeMemory(m_device->logicalDevice, indices.memory, nullptr);
    }
    for (auto texture : textures) {
        texture.destroy();
    }
}

void Scene::draw(VkCommandBuffer t_commandBuffer, VkPipelineLayout t_pipelineLayout,
    uint32_t t_firstBinding) const
{
    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(t_commandBuffer, t_firstBinding, 1, &vertices.buffer, offsets);
    vkCmdBindIndexBuffer(t_commandBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);

    // TODO: iterate over instances instead of meshes,
    //  generate instances analogously to the ray tracing pipeline.
    for (const auto mesh : meshes) {
        // Render from the global scene vertex buffer using the mesh index offset
        auto materialIdx = mesh.getMaterialIdx();
        vkCmdPushConstants(t_commandBuffer,
            t_pipelineLayout,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(uint32_t),
            &materialIdx);
        vkCmdDrawIndexed(t_commandBuffer,
            mesh.getIndexCount(),
            1,
            mesh.getIndexBase(),
            mesh.getVertexBase(),
            0);
    }
}

bool Scene::loadFromFile(const std::string& t_modelPath, const SceneVertexLayout& t_layout,
    SceneCreateInfo* t_createInfo, Device* t_device, VkQueue t_copyQueue)
{
    this->m_device = t_device;
    this->m_vertexLayout = t_layout;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile((t_modelPath).c_str(), defaultFlags);
    if (!scene) {
        m_error = true;
        throw std::logic_error("Error loading assets: " + std::string(importer.GetErrorString()));
    } else {
        loadCamera(scene);
        loadLights(scene);
        loadMaterials(scene, t_copyQueue);

        meshes.clear();
        meshes.resize(scene->mNumMeshes);

        glm::vec3 scale(1.0f);
        glm::vec2 uvscale(1.0f);
        glm::vec3 center(0.0f);
        if (t_createInfo) {
            scale = t_createInfo->scale;
            uvscale = t_createInfo->uvScale;
            center = t_createInfo->center;
        }

        std::vector<float> vertexBuffer;
        std::vector<uint32_t> indexBuffer;

        vertexCount = 0;
        indexCount = 0;
        // Load meshes (and instances for each mesh)
        std::cout << "\nLoading Meshes..." << std::endl;
        const auto length = static_cast<float>(scene->mNumMeshes);
        for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
            const aiMesh* pAiMesh = scene->mMeshes[i];
            auto currentIndexOffset = static_cast<uint32_t>(indexBuffer.size()) * sizeof(uint32_t);
            auto currentVertexOffset = static_cast<uint32_t>(vertexBuffer.size()) * sizeof(float);

            const aiVector3D zero3D(0.0f, 0.0f, 0.0f);

            for (unsigned int j = 0; j < pAiMesh->mNumVertices; j++) {
                const aiVector3D* pPos = &(pAiMesh->mVertices[j]);
                const aiVector3D* pNormal = &(pAiMesh->mNormals[j]);
                const aiVector3D* pTexCoord
                    = (pAiMesh->HasTextureCoords(0)) ? &(pAiMesh->mTextureCoords[0][j]) : &zero3D;
                const aiVector3D* pTangent
                    = (pAiMesh->HasTangentsAndBitangents()) ? &(pAiMesh->mTangents[j]) : &zero3D;
                const aiVector3D* pBiTangent
                    = (pAiMesh->HasTangentsAndBitangents()) ? &(pAiMesh->mBitangents[j]) : &zero3D;

                for (auto& component : t_layout.components) {
                    switch (component) {
                    case VERTEX_COMPONENT_POSITION:
                        vertexBuffer.push_back(pPos->x * scale.x + center.x);
                        vertexBuffer.push_back(-pPos->y * scale.y + center.y);
                        vertexBuffer.push_back(pPos->z * scale.z + center.z);
                        break;
                    case VERTEX_COMPONENT_NORMAL:
                        vertexBuffer.push_back(pNormal->x);
                        vertexBuffer.push_back(-pNormal->y);
                        vertexBuffer.push_back(pNormal->z);
                        break;
                    case VERTEX_COMPONENT_UV:
                        vertexBuffer.push_back(pTexCoord->x * uvscale.s);
                        vertexBuffer.push_back(pTexCoord->y * uvscale.t);
                        break;
                    case VERTEX_COMPONENT_TANGENT:
                        if (std::isnan(pTangent->x) || std::isnan(pTangent->y)
                            || std::isnan(pTangent->z)) {
                            vertexBuffer.push_back(1.f);
                            vertexBuffer.push_back(1.f);
                            vertexBuffer.push_back(1.f);
                        } else {
                            vertexBuffer.push_back(pTangent->x);
                            vertexBuffer.push_back(pTangent->y);
                            vertexBuffer.push_back(pTangent->z);
                        }
                        break;
                    case VERTEX_COMPONENT_BITANGENT:
                        vertexBuffer.push_back(pBiTangent->x);
                        vertexBuffer.push_back(pBiTangent->y);
                        vertexBuffer.push_back(pBiTangent->z);
                        break;
                    case VERTEX_COMPONENT_DUMMY_FLOAT:
                        vertexBuffer.push_back(0.0f);
                        break;
                    case VERTEX_COMPONENT_DUMMY_VEC4:
                        vertexBuffer.push_back(0.0f);
                        vertexBuffer.push_back(0.0f);
                        vertexBuffer.push_back(0.0f);
                        vertexBuffer.push_back(0.0f);
                        break;
                    };
                }

                dim.max.x = fmax(pPos->x, dim.max.x);
                dim.max.y = fmax(pPos->y, dim.max.y);
                dim.max.z = fmax(pPos->z, dim.max.z);

                dim.min.x = fmin(pPos->x, dim.min.x);
                dim.min.y = fmin(pPos->y, dim.min.y);
                dim.min.z = fmin(pPos->z, dim.min.z);
            }

            dim.size = dim.max - dim.min;

            uint32_t meshIndexCount = 0;

            for (unsigned int j = 0; j < pAiMesh->mNumFaces; j++) {
                const aiFace& face = pAiMesh->mFaces[j];
                for (unsigned int k = 0; k < face.mNumIndices; k++) {
                    indexBuffer.push_back(face.mIndices[k]);
                    meshIndexCount += 1;
                }
            }
            uint32_t meshIndexBase = indexCount;
            indexCount += meshIndexCount;
            uint32_t meshVertexBase = vertexCount;
            vertexCount += pAiMesh->mNumVertices;

            meshes[i] = Mesh(i,
                currentIndexOffset,
                meshIndexBase,
                meshIndexCount,
                currentVertexOffset,
                meshVertexBase,
                pAiMesh->mNumVertices,
                pAiMesh->mMaterialIndex);

            if (m_materials[meshes[i].getMaterialIdx()].isEmissive()) {
                Light areaLight(i, meshes[i].getMaterialIdx(), pAiMesh->mNumFaces);
                m_lights.emplace_back(areaLight);
            }
            debug::printPercentage(i, length);
        }
        std::cout << "\nGenerating mesh buffers..." << std::endl;

        uint32_t vBufferSize = static_cast<uint32_t>(vertexBuffer.size()) * sizeof(float);
        uint32_t iBufferSize = static_cast<uint32_t>(indexBuffer.size()) * sizeof(uint32_t);

        // Use staging buffer to move vertex and index buffer to device local memory
        // Create staging buffers
        Buffer vertexStaging, indexStaging;

        // Vertex buffer
        vertexStaging.create(t_device,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            vBufferSize,
            vertexBuffer.data());
        // Index buffer
        indexStaging.create(t_device,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            iBufferSize,
            indexBuffer.data());

        // Create device local target buffers
        // Vertex buffer
        vertices.create(t_device,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | t_createInfo->memoryPropertyFlags,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            vBufferSize);
        // Index buffer
        indices.create(t_device,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | t_createInfo->memoryPropertyFlags,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            iBufferSize);
        // Copy from staging buffers
        VkCommandBuffer copyCmd
            = t_device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        VkBufferCopy copyRegion {};

        copyRegion.size = vBufferSize;
        vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, vertices.buffer, 1, &copyRegion);

        copyRegion.size = iBufferSize;
        vkCmdCopyBuffer(copyCmd, indexStaging.buffer, indices.buffer, 1, &copyRegion);

        t_device->flushCommandBuffer(copyCmd, t_copyQueue);
        // Destroy staging resources
        vkDestroyBuffer(t_device->logicalDevice, vertexStaging.buffer, nullptr);
        vkFreeMemory(t_device->logicalDevice, vertexStaging.memory, nullptr);
        vkDestroyBuffer(t_device->logicalDevice, indexStaging.buffer, nullptr);
        vkFreeMemory(t_device->logicalDevice, indexStaging.memory, nullptr);

        debug::printPercentage(0, 1);
        m_loaded = true;
        return true;
    }
}

void Scene::loadCamera(const aiScene* t_scene)
{
    if (t_scene->HasCameras()) {
        m_camera = Camera(*t_scene->mCameras[0]); // Only one camera supported
    }
}

Camera* Scene::getCamera() { return &m_camera; }

void Scene::loadLights(const aiScene* t_scene)
{
    if (t_scene->HasLights()) {
        std::cout << "\nLoading Lights..." << std::endl;
        for (unsigned int i = 0; i < t_scene->mNumLights; i++) {
            const auto aiLight = t_scene->mLights[i];
            m_lights.emplace_back(*aiLight);
            debug::printPercentage(i, t_scene->mNumLights);
        }
    }
}

std::vector<ShaderLight> Scene::getLightsShaderData()
{
    std::vector<ShaderLight> lights;
    for (auto& light : m_lights) {
        lights.emplace_back(light.getShaderLight());
    }
    return lights;
}

size_t Scene::getLightCount() { return m_lights.size(); }

void Scene::loadMaterials(const aiScene* t_scene, VkQueue t_transferQueue)
{
    m_materials.resize(t_scene->mNumMaterials);
    std::cout << "\nLoading Materials..." << std::endl;
    const auto length = static_cast<float>(m_materials.size());
    for (size_t i = 0; i < m_materials.size(); i++) {
        m_materials[i] = Material(m_device, t_transferQueue, this, t_scene, t_scene->mMaterials[i]);
        debug::printPercentage(i, length);
    }
}

std::vector<ShaderMaterial> Scene::getMaterialsShaderData()
{
    std::vector<ShaderMaterial> materials;
    for (auto& material : m_materials) {
        materials.emplace_back(material.getShaderMaterial());
    }
    return materials;
}

size_t Scene::getMaterialCount() { return m_materials.size(); }

size_t Scene::getTexturesCount() { return textures.size(); }

std::vector<ShaderMeshInstance> Scene::getInstancesShaderData()
{
    std::vector<ShaderMeshInstance> dataInstances;
    for (auto& instance : instances) {
        auto mesh = meshes[instance.getMeshIdx()];
        ShaderMeshInstance vulkanMeshInstance {};
        vulkanMeshInstance.materialIndex = mesh.getMaterialIdx();
        vulkanMeshInstance.vertexBase = mesh.getVertexBase();
        vulkanMeshInstance.indexBase = mesh.getIndexBase();
        dataInstances.emplace_back(vulkanMeshInstance);
    }
    return dataInstances;
}

size_t Scene::getInstancesCount() { return instances.size(); }

void Scene::createMeshInstance(uint32_t t_blasIdx, uint32_t t_meshIdx)
{
    instances.emplace_back(Instance(t_blasIdx, t_meshIdx));
}

bool Scene::isLoaded() { return m_loaded || m_error; }

uint32_t Scene::getVertexLayoutStride() { return m_vertexLayout.stride(); }
