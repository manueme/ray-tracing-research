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
        VkPipeline postProcess;
    } m_pipelines;
    struct {
        VkPipelineLayout rayTracing;
        VkPipelineLayout postProcess;
    } m_pipelineLayouts;

    struct {
        VkDescriptorSet set0AccelerationStructure;
        std::vector<VkDescriptorSet> set1Scene;
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
        std::vector<VkDescriptorSet> set0Scene;
        VkDescriptorSet set1InputImage;
    } m_postprocessDescriptorSets;
    struct {
        VkDescriptorSetLayout set0Scene;
        VkDescriptorSetLayout set1InputImage;
    } m_postprocessDescriptorSetLayouts;
    Buffer m_instancesBuffer;
    Buffer m_lightsBuffer;
    Buffer m_materialsBuffer;

    // Images used to store ray traced image
    struct {
        Texture result;
        // - Normal map, Depth Map and Albedo are used by the denoiser in the denoiser app,
        // can be removed if you don't need them.
        // - The depth map is also used for the DOF effect
        Texture depthMap;
        Texture normalMap;
        Texture albedo;
    } m_storageImage;

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
    void createDescriptorPool();
    void createDescriptorSetsLayout();
    void assignPushConstants();
    void createDescriptorSets();
    void updateResultImageDescriptorSets();
    void createUniformBuffers();
    void createRTPipeline();
    void createShaderRTBindingTable();
    void createPostprocessPipeline();

    void getEnabledFeatures() override;
};

#endif // MANUEME_MONTE_CARLO_RAY_TRACING_H
