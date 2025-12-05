/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_SCENE_H
#define MANUEME_SCENE_H

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <assimp/Importer.hpp>
#include <cstdlib>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <vector>

#include "../core/buffer.h"
#include "../core/device.h"
#include "../core/texture.h"
#include "./shader_light.h"
#include "camera.h"
#include "instance.h"
#include "light.h"
#include "material.h"
#include "mesh.h"
#include "shader_instance.h"
#include "vulkan/vulkan.h"

/** @brief Vertex layout components */
enum Component {
    VERTEX_COMPONENT_POSITION = 0x0,
    VERTEX_COMPONENT_NORMAL = 0x1,
    VERTEX_COMPONENT_UV = 0x2,
    VERTEX_COMPONENT_TANGENT = 0x3,
    VERTEX_COMPONENT_BITANGENT = 0x4,
    VERTEX_COMPONENT_DUMMY_FLOAT = 0x5,
    VERTEX_COMPONENT_DUMMY_VEC4 = 0x6
};

/** @brief Stores vertex layout components for model loading and Vulkan vertex
 * input and attribute bindings  */
class SceneVertexLayout {
public:
    explicit SceneVertexLayout(std::vector<Component> t_components = {})
        : components(std::move(t_components)) { };
    ~SceneVertexLayout() = default;
    uint32_t stride();
    std::vector<Component> components;
};

/** @brief Used to parametrize model loading */
class SceneCreateInfo {
public:
    glm::vec3 center;
    glm::vec3 scale;
    glm::vec2 uvScale;
    VkMemoryPropertyFlags memoryPropertyFlags = 0;
    SceneCreateInfo();
    ~SceneCreateInfo();
    SceneCreateInfo(glm::vec3 t_scale, glm::vec2 t_uvScale, glm::vec3 t_center);
    SceneCreateInfo(float t_scale, float t_uvScale, float t_center);
};

class Scene {
public:
    Scene();
    ~Scene();

    /** @brief Release all Vulkan resources of this model,
     * this function is NOT called by the destructor of the class */
    void destroy();

    Buffer vertices;
    Buffer indices;
    uint32_t indexCount = 0;
    uint32_t vertexCount = 0;

    void draw(VkCommandBuffer t_commandBuffer, VkPipelineLayout t_pipelineLayout,
        uint32_t t_firstBinding) const;

    std::vector<Instance> instances;
    std::vector<Mesh> meshes;
    std::vector<Texture> textures;

    struct Dimension {
        glm::vec3 min = glm::vec3(FLT_MAX);
        glm::vec3 max = glm::vec3(-FLT_MAX);
        glm::vec3 size;
    } dim;

    bool loadFromFile(const std::string& t_modelPath, const SceneVertexLayout& t_layout,
        SceneCreateInfo* t_createInfo, Device* t_device, VkQueue t_copyQueue);

    Camera* getCamera();

    std::vector<ShaderLight> getLightsShaderData();
    size_t getLightCount();

    std::vector<ShaderMaterial> getMaterialsShaderData();
    size_t getMaterialCount();

    size_t getTexturesCount();

    void createMeshInstance(uint32_t t_blasIdx, uint32_t t_meshIdx);
    std::vector<ShaderMeshInstance> getInstancesShaderData();
    size_t getInstancesCount();

    bool isLoaded();

    uint32_t getVertexLayoutStride();

private:
    static const int defaultFlags = aiProcess_FlipWindingOrder | aiProcess_PreTransformVertices
        | aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals
        | aiProcess_EmbedTextures | aiProcess_JoinIdenticalVertices
        | aiProcess_ValidateDataStructure;

    Device* m_device = nullptr;

    SceneVertexLayout m_vertexLayout;

    bool m_loaded = false;
    bool m_error = false;

    std::vector<Material> m_materials;
    std::vector<Light> m_lights;
    Camera m_camera;

    void loadCamera(const aiScene* t_scene);

    void loadLights(const aiScene* t_scene);

    void loadMaterials(const aiScene* t_scene, VkQueue t_transferQueue);
};

#endif // MANUEME_SCENE_H
