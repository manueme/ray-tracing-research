/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_MONTE_CARLO_RAY_TRACING_H
#define MANUEME_MONTE_CARLO_RAY_TRACING_H

#include "base_rt_project.h"
#include "core/texture.h"

class MonteCarloRTApp : public BaseRTProject {
public:
    MonteCarloRTApp();
    ~MonteCarloRTApp();

private:
    struct {
        VkPipeline rayTracing;
        VkPipeline raster;
    } m_pipelines;
    struct {
        VkPipelineLayout rayTracing;
        VkPipelineLayout raster;
    } m_pipelineLayouts;

    struct {
        VkDescriptorSet set0AccelerationStructure;
        std::vector<VkDescriptorSet> set1Scene;
        VkDescriptorSet set2Geometry;
        VkDescriptorSet set3Materials;
        VkDescriptorSet set4Lights;
        VkDescriptorSet set5ResultImage;
        VkDescriptorSet set6AuxImage;
    } m_rtDescriptorSets;
    struct {
        VkDescriptorSetLayout set0AccelerationStructure;
        VkDescriptorSetLayout set1Scene;
        VkDescriptorSetLayout set2Geometry;
        VkDescriptorSetLayout set3Materials;
        VkDescriptorSetLayout set4Lights;
        VkDescriptorSetLayout set5ResultImage;
        VkDescriptorSetLayout set6AuxImage;
    } m_rtDescriptorSetLayouts;
    struct {
        std::vector<VkDescriptorSet> set0Scene;
        VkDescriptorSet set1InputImage;
        VkDescriptorSet set2ConvolutionKernels;
    } m_rasterDescriptorSets;
    struct {
        VkDescriptorSetLayout set0Scene;
        VkDescriptorSetLayout set1InputImage;
        VkDescriptorSetLayout set2ConvolutionKernels;
    } m_rasterDescriptorSetLayouts;
    Buffer m_instancesBuffer;
    Buffer m_lightsBuffer;
    Buffer m_materialsBuffer;

    // Images used to store ray traced image
    struct {
        VulkanTexture2D color;
        VulkanTexture2D depthMap;
        VulkanTexture2D firstSample;
    } m_storageImage;

    // Unused for now
    struct {
        VulkanTexture2D rgbNoise;
    } m_auxImages;

    struct {
        VulkanTexture2D layer1;
    } m_convolutionKernels;

    struct UniformData {
        glm::mat4 viewInverse { glm::mat4(0.0) };
        glm::mat4 projInverse { glm::mat4(0.0) };
        glm::vec4 overrideSunDirection { glm::vec4(0.0) };
        int frameIteration { 0 }; // Current frame iteration number
        int frame { 0 }; // Current frame
        int frameChanged { 1 }; // Current frame changed size
    } m_sceneUniformData;
    std::vector<Buffer> m_sceneBuffers;

    const int m_ray_tracer_depth = 8;
    const int m_ray_tracer_samples = 1;
    // Push constant sent to the path tracer
    struct PathTracerParameters {
        int maxDepth; // Max depth
        int samples; // samples per frame
    } m_pathTracerParams;

    SceneVertexLayout m_vertexLayout = SceneVertexLayout({ VERTEX_COMPONENT_POSITION,
        VERTEX_COMPONENT_NORMAL,
        VERTEX_COMPONENT_TANGENT,
        VERTEX_COMPONENT_UV,
        VERTEX_COMPONENT_DUMMY_FLOAT });

    Buffer m_shaderBindingTable;

    void render() override;
    void prepare() override;
    void viewChanged() override;
    void windowResized() override;
    void updateUniformBuffers(uint32_t t_currentImage) override;
    void onSwapChainRecreation() override;
    void buildCommandBuffers() override;
    void onKeyEvent(int t_key, int t_scancode, int t_action, int t_mods) override;
    void createStorageImages();
    void createConvolutionKernels();
    void createDescriptorPool();
    void createDescriptorSetsLayout();
    void assignPushConstants();
    void createDescriptorSets();
    void updateResultImageDescriptorSets();
    void createUniformBuffers();
    void createRTPipeline();
    void createRasterPipeline();
    void createShaderRTBindingTable();

    void getEnabledFeatures() override;
};

#endif // MANUEME_MONTE_CARLO_RAY_TRACING_H
