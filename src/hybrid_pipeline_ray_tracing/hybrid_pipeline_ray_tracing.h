/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_HYBRID_PIPELINE_RAY_TRACING_H
#define MANUEME_HYBRID_PIPELINE_RAY_TRACING_H

#include "base_rt_project.h"

class HybridPipelineRT : public BaseRTProject {
public:
    HybridPipelineRT();
    ~HybridPipelineRT();

private:

    void viewChanged() override;

    const uint32_t vertex_buffer_bind_id = 0;

    struct {
        VkPipeline raster;
    } m_pipelines;
    struct {
        VkPipelineLayout raster;
    } m_pipelineLayouts;

    struct {
        std::vector<VkDescriptorSet> set0Scene;
        VkDescriptorSet set1Materials;
        VkDescriptorSet set2Lights;
    } m_rasterDescriptorSets;
    struct {
        VkDescriptorSetLayout set0Scene;
        VkDescriptorSetLayout set1Materials;
        VkDescriptorSetLayout set2Lights;
    } m_rasterDescriptorSetLayouts;
    Buffer m_lightsBuffer;
    Buffer m_materialsBuffer;

    struct {
        glm::mat4 projection;
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 viewInverse { glm::mat4(1.0) };
        glm::mat4 projInverse { glm::mat4(1.0) };
        glm::vec4 overrideSunDirection { glm::vec4(0.0) };
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
    void updateUniformBuffers(uint32_t t_currentImage) override;
    void buildCommandBuffers() override;
    void onKeyEvent(int t_key, int t_scancode, int t_action, int t_mods) override;
    void createDescriptorPool();
    void createDescriptorSets();
    void createDescriptorSetLayout();
    void createUniformBuffers();
    void createRasterPipeline();

    void getEnabledFeatures() override;
};

#endif // MANUEME_HYBRID_PIPELINE_RAY_TRACING_H
