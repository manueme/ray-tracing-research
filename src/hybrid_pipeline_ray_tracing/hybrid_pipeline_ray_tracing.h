/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_HYBRID_PIPELINE_RAY_TRACING_H
#define MANUEME_HYBRID_PIPELINE_RAY_TRACING_H

#include "base_project.h"
#include "core/texture.h"

class HyRayTracingPipeline;

class HybridPipelineRT : public BaseProject {
public:
    HybridPipelineRT();
    ~HybridPipelineRT();

private:
    HyRayTracingPipeline* m_rayTracing;

    const uint32_t vertex_buffer_bind_id = 0;

    // PIPELINES
    struct {
        VkPipeline raster;
        VkPipeline postProcess;
    } m_pipelines;
    struct {
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

    void render() override;
    void setupScene();
    void prepare() override;
    void viewChanged() override;
    void createOffscreenRenderPass();
    void updateUniformBuffers(uint32_t t_currentImage);
    void onSwapChainRecreation() override;
    void createOffscreenFramebuffers();
    void buildCommandBuffers() override;
    void onKeyEvent(int t_key, int t_scancode, int t_action, int t_mods) override;
    void createStorageImages();
    void createRTPipeline();
    void createRasterPipeline();
    void createPostprocessPipeline();
    void createDescriptorPool();
    void createDescriptorSetLayout();
    void createDescriptorSets();
    void updateResultImageDescriptorSets();
    void createUniformBuffers();
};

#endif // MANUEME_HYBRID_PIPELINE_RAY_TRACING_H
