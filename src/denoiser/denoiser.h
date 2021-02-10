/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_DENOISER_H
#define MANUEME_DENOISER_H

#include "base_rt_project.h"
#include "core/texture.h"

class DenoiserApp : public BaseRTProject {
public:
    DenoiserApp();
    ~DenoiserApp();

private:
    struct {
        VkPipeline rayTracing;
        VkPipeline postProcess;
        VkPipeline trainDenoise;
    } m_pipelines;
    struct {
        VkPipelineLayout rayTracing;
        VkPipelineLayout postProcess;
        VkPipelineLayout trainDenoise;
    } m_pipelineLayouts;

    struct {
        VkQueue queue;
        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;
        VkFence fence;
    } m_compute;

    struct {
        VkDescriptorSet set0AccelerationStructure;
        VkDescriptorSet set1Scene;
        VkDescriptorSet set2Geometry;
        VkDescriptorSet set3Materials;
        VkDescriptorSet set4Lights;
        VkDescriptorSet set5ResultImage;
    } m_rtDescriptorSets;
    struct {
        VkDescriptorSetLayout set0AccelerationStructure;
        VkDescriptorSetLayout set1Scene;
        VkDescriptorSetLayout set2Geometry;
        VkDescriptorSetLayout set3Materials;
        VkDescriptorSetLayout set4Lights;
        VkDescriptorSetLayout set5ResultImage;
    } m_rtDescriptorSetLayouts;
    struct {
        VkDescriptorSet set0Scene;
        VkDescriptorSet set1InputImage;
    } m_postprocessDescriptorSets;
    struct {
        VkDescriptorSetLayout set0Scene;
        VkDescriptorSetLayout set1InputImage;
    } m_postprocessDescriptorSetLayouts;
    struct {
        VkDescriptorSet set0Scene;
        VkDescriptorSet set1InputImage;
        VkDescriptorSet set2Minibatch;
    } m_trainDenoiseDescriptorSets;
    struct {
        VkDescriptorSetLayout set0Scene;
        VkDescriptorSetLayout set1InputImage;
        VkDescriptorSetLayout set2Minibatch;
    } m_trainDenoiseDescriptorSetLayouts;

    Buffer m_instancesBuffer;
    Buffer m_lightsBuffer;
    Buffer m_materialsBuffer;

    // Images used to store ray traced image
    struct {
        Texture result;
        Texture depthMap;
        Texture normalMap;
        Texture albedo;
    } m_storageImage;

    // Denoiser minibatch storage image
    const uint32_t m_minibatch_size = 8;
    struct {
        Texture accumulatedSample;
        Texture rawSample;
        Texture depthMap;
        Texture normalMap;
        Texture albedo;
    } m_minibatch;

    struct UniformData {
        glm::mat4 viewInverse { glm::mat4(0.0) };
        glm::mat4 projInverse { glm::mat4(0.0) };
        glm::vec4 overrideSunDirection { glm::vec4(0.0) };
        uint32_t frameIteration { 0 }; // Current frame iteration number
        uint32_t frame { 0 }; // Current frame
        uint32_t frameChanged { 1 }; // Current frame changed size
    } m_sceneUniformData;
    Buffer m_sceneBuffer;

    const uint32_t m_ray_tracer_depth = 4;
    const uint32_t m_ray_tracer_samples = 1;
    // Push constant sent to the path tracer
    struct PathTracerParameters {
        uint32_t maxDepth; // Max depth
        uint32_t samples; // samples per frame
    } m_pathTracerParams;

    SceneVertexLayout m_vertexLayout = SceneVertexLayout({ VERTEX_COMPONENT_POSITION,
        VERTEX_COMPONENT_NORMAL,
        VERTEX_COMPONENT_TANGENT,
        VERTEX_COMPONENT_UV,
        VERTEX_COMPONENT_DUMMY_FLOAT });

    Buffer m_shaderBindingTable;

    // Prepare compute
    void prepareCompute();
    void createComputeCommandBuffers();
    void freeComputeCommandBuffers();
    // ---

    void render() override;
    void prepare() override;
    void viewChanged() override;
    void windowResized() override;
    void updateUniformBuffers(uint32_t t_currentImage) override;
    void onSwapChainRecreation() override;
    void buildCommandBuffers() override;
    void onKeyEvent(int t_key, int t_scancode, int t_action, int t_mods) override;
    void createStorageImages();
    void createDescriptorPool();
    void createDescriptorSetsLayout();
    void assignPushConstants();
    void createDescriptorSets();
    void updateResultImageDescriptorSets();
    void createUniformBuffers();
    void createRTPipeline();
    void createShaderRTBindingTable();
    void createPostprocessPipeline();
    void createComputeDenoisePipelines();

    void getEnabledFeatures() override;
};

#endif // MANUEME_DENOISER_H
