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

    // PIPELINES
    struct {
        VkPipeline rayTracing;
        VkPipeline raster;
        VkPipeline postProcess;
    } m_pipelines;
    struct {
        VkPipelineLayout rayTracing;
        VkPipelineLayout raster;
        VkPipelineLayout postProcess;
    } m_pipelineLayouts;

    // RASTER SETS
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
    // RAY TRACING SETS
    struct {
        VkDescriptorSet set0AccelerationStructure;
        std::vector<VkDescriptorSet> set1Scene;
        VkDescriptorSet set2Geometry;
        VkDescriptorSet set3Materials;
        VkDescriptorSet set4Lights;
        std::vector<VkDescriptorSet> set5OffscreenImages;
        std::vector<VkDescriptorSet> set6StorageImages;
    } m_rtDescriptorSets;
    struct {
        VkDescriptorSetLayout set0AccelerationStructure;
        VkDescriptorSetLayout set1Scene;
        VkDescriptorSetLayout set2Geometry;
        VkDescriptorSetLayout set3Materials;
        VkDescriptorSetLayout set4Lights;
        VkDescriptorSetLayout set5OffscreenImages;
        VkDescriptorSetLayout set6StorageImages;
    } m_rtDescriptorSetLayouts;
    // POSTPROCESS SETS
    struct {
        std::vector<VkDescriptorSet> set0StorageImages;
    } m_postProcessDescriptorSets;
    struct {
        VkDescriptorSetLayout set0StorageImages;
    } m_postProcessDescriptorSetLayouts;

    Buffer m_instancesBuffer;
    Buffer m_lightsBuffer;
    Buffer m_materialsBuffer;

    // Images used to store ray traced image
    struct OffscreenImages {
        Texture offscreenColor;
        Texture offscreenNormals;
        Texture offscreenDepth;
        Texture offscreenReflectRefractMap;
        Texture rtResultImage;
    };
    // we will have one set of images per swap image
    std::vector<OffscreenImages> m_offscreenImages;
    // Offscreen raster render pass
    VkRenderPass m_offscreenRenderPass;
    std::vector<VkFramebuffer> m_offscreenFramebuffers;
    // ---


    struct {
        glm::mat4 projection;
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 viewInverse { glm::mat4(1.0) };
        glm::mat4 projInverse { glm::mat4(1.0) };
        glm::vec4 overrideSunDirection { glm::vec4(0.0) };
        uint32_t frame { 0 }; // Current frame
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

    const uint32_t m_ray_tracer_depth = 8;
    const uint32_t m_ray_tracer_samples = 1;
    // Push constant sent to the path tracer
    struct PathTracerParameters {
        uint32_t maxDepth; // Max depth
        uint32_t samples; // samples per frame
    } m_pathTracerParams;

    void render() override;
    void prepare() override;

    void createOffscreenRenderPass();
    void updateUniformBuffers(uint32_t t_currentImage) override;
    void onSwapChainRecreation() override;
    void createStorageImages();
    void createOffscreenFramebuffers();
    void onKeyEvent(int t_key, int t_scancode, int t_action, int t_mods) override;

    void buildCommandBuffers() override;

    void createRTPipeline();
    void createRasterPipeline();
    void createPostprocessPipeline();
    void createShaderRTBindingTable();

    void createDescriptorPool();
    void createDescriptorSetLayout();
    void createDescriptorSets();
    void updateResultImageDescriptorSets();

    void assignPushConstants();
    void createUniformBuffers();

    void getEnabledFeatures() override;
};

#endif // MANUEME_HYBRID_PIPELINE_RAY_TRACING_H
