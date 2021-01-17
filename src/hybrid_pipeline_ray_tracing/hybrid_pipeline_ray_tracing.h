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
        VkPipeline rayTracing;
        VkPipeline raster;
    } m_pipelines;
    struct {
        VkPipelineLayout rayTracing;
        VkPipelineLayout raster;
    } m_pipelineLayouts;

    struct {
        std::vector<VkDescriptorSet> set0Scene;
        VkDescriptorSet set1Materials;
        VkDescriptorSet set2Lights;
        VkDescriptorSet set3StorageImage;
    } m_rasterDescriptorSets;
    struct {
        VkDescriptorSetLayout set0Scene;
        VkDescriptorSetLayout set1Materials;
        VkDescriptorSetLayout set2Lights;
        VkDescriptorSetLayout set3StorageImage;
    } m_rasterDescriptorSetLayouts;
    struct {
        VkDescriptorSet set0AccelerationStructure;
        std::vector<VkDescriptorSet> set1Scene;
        VkDescriptorSet set2Geometry;
        VkDescriptorSet set3Materials;
        VkDescriptorSet set4Lights;
        VkDescriptorSet set5StorageImage;
    } m_rtDescriptorSets;
    struct {
        VkDescriptorSetLayout set0AccelerationStructure;
        VkDescriptorSetLayout set1Scene;
        VkDescriptorSetLayout set2Geometry;
        VkDescriptorSetLayout set3Materials;
        VkDescriptorSetLayout set4Lights;
        VkDescriptorSetLayout set5StorageImage;
    } m_rtDescriptorSetLayouts;
    Buffer m_instancesBuffer;
    Buffer m_lightsBuffer;
    Buffer m_materialsBuffer;

    // Images used to store ray traced image
    struct {
        VulkanTexture2D rtInputColor;
        VulkanTexture2D rtInputNormals;
        VulkanTexture2D rtInputDepth;
        VulkanTexture2D resultImage;
    } m_storageImage;

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

    Buffer m_shaderBindingTable;

    const int m_ray_tracer_depth = 8;
    const int m_ray_tracer_samples = 1;
    // Push constant sent to the path tracer
    struct PathTracerParameters {
        int maxDepth; // Max depth
        int samples; // samples per frame
    } m_pathTracerParams;

    void render() override;
    void prepare() override;
    void updateUniformBuffers(uint32_t t_currentImage) override;
    void onSwapChainRecreation() override;
    void buildCommandBuffers() override;
    void createStorageImages();
    void onKeyEvent(int t_key, int t_scancode, int t_action, int t_mods) override;
    void createDescriptorPool();
    void createDescriptorSets();
    void updateResultImageDescriptorSets();
    void createDescriptorSetLayout();
    void assignPushConstants();
    void createUniformBuffers();
    void createRTPipeline();
    void createRasterPipeline();
    void createShaderRTBindingTable();
    void getEnabledFeatures() override;
};

#endif // MANUEME_HYBRID_PIPELINE_RAY_TRACING_H
