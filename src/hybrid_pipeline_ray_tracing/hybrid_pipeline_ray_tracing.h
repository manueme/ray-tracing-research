/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_HYBRID_PIPELINE_RAY_TRACING_H
#define MANUEME_HYBRID_PIPELINE_RAY_TRACING_H

#include "base_project.h"
#include "scene/scene.h"

class HybridPipelineRT : public BaseProject {
public:
    HybridPipelineRT();
    ~HybridPipelineRT();

private:

    void viewChanged() override;

    const uint32_t vertex_buffer_bind_id = 0;

    // Device extra features
    VkPhysicalDeviceDescriptorIndexingFeaturesEXT m_indexFeature {};
    VkPhysicalDeviceBufferDeviceAddressFeatures m_bufferDeviceAddressFeatures {};

    struct {
        VkPipeline m_models;
    } m_pipelines;
    VkPipelineLayout m_pipelineLayout;

    struct {
        std::vector<VkDescriptorSet> set0Scene;
        VkDescriptorSet set1Materials;
        VkDescriptorSet set2Lights;
    } m_descriptorSets;
    struct {
        VkDescriptorSetLayout set0Scene;
        VkDescriptorSetLayout set1Materials;
        VkDescriptorSetLayout set2Lights;
    } m_descriptorSetLayouts;
    Buffer m_lightsBuffer;
    Buffer m_materialsBuffer;

    struct {
        glm::mat4 m_projection;
        glm::mat4 m_model;
        glm::mat4 m_view;
        glm::mat4 m_viewInverse;
    } m_sceneUniformData;
    // one for each swap chain image, the scene can change on every frame
    std::vector<Buffer> m_sceneBuffers;

    // Vertex layout for the models
    SceneVertexLayout m_vertexLayout = SceneVertexLayout({ VERTEX_COMPONENT_POSITION,
        VERTEX_COMPONENT_NORMAL,
        VERTEX_COMPONENT_TANGENT,
        VERTEX_COMPONENT_UV,
        VERTEX_COMPONENT_DUMMY_FLOAT });

    void render() override;
    void prepare() override;
    void loadAssets();
    void updateUniformBuffers(uint32_t t_currentImage) override;
    void buildCommandBuffers() override;
    void createDescriptorPool();
    void createDescriptorSet();
    void createDescriptorSetLayout();
    void createUniformBuffers();
    void createPipelines();

    void getEnabledFeatures() override;
};

#endif // MANUEME_HYBRID_PIPELINE_RAY_TRACING_H
