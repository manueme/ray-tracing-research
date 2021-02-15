/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_DENOISER_H
#define MANUEME_DENOISER_H

#include "base_project.h"
#include "core/texture.h"

class RayTracingPipeline;
class AutoExposurePipeline;
class PostProcessPipeline;

class DenoiserApp : public BaseProject {
public:
    DenoiserApp();
    ~DenoiserApp();

private:
    RayTracingPipeline* m_rayTracing;
    AutoExposurePipeline* m_autoExposure;
    PostProcessPipeline* m_postProcess;

    struct {
        VkPipeline predictDenoise;
        // TODO: add backpropagate denoise
    } m_pipelines;
    struct {
        VkPipelineLayout predictDenoise;
    } m_pipelineLayouts;

    struct {
        VkDescriptorSet set0Scene;
        VkDescriptorSet set1InputImage;
        VkDescriptorSet set2Minibatch;
    } m_predictDenoiseDescriptorSets;
    struct {
        VkDescriptorSetLayout set0Scene;
        VkDescriptorSetLayout set1InputImage;
        VkDescriptorSetLayout set2Minibatch;
    } m_predictDenoiseDescriptorSetLayouts;

    // Images used to store ray traced image
    struct {
        Texture result;
        Texture postProcessResult;
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
        Texture denoiseOutput;
    } m_minibatch;

    Buffer m_instancesBuffer;
    Buffer m_lightsBuffer;
    Buffer m_materialsBuffer;

    struct UniformData {
        glm::mat4 viewInverse { glm::mat4(0.0) };
        glm::mat4 projInverse { glm::mat4(0.0) };
        glm::vec4 overrideSunDirection { glm::vec4(0.0) };
        uint32_t frameIteration { 0 }; // Current frame iteration number
        uint32_t frame { 0 }; // Current frame
        uint32_t frameChanged { 1 }; // Current frame changed size
        float manualExposureAdjust = { 1.0f };
    } m_sceneUniformData;
    Buffer m_sceneBuffer;

    struct ExposureUniformData {
        float exposure = { 1.0f };
    } m_exposureData;
    Buffer m_exposureBuffer;

    void render() override;
    void setupScene();
    void prepare() override;
    void viewChanged() override;
    void windowResized() override;
    void updateUniformBuffers(uint32_t t_currentImage);
    void onSwapChainRecreation() override;
    void buildCommandBuffers() override;
    void onKeyEvent(int t_key, int t_scancode, int t_action, int t_mods) override;
    void createStorageImages();
    void createDescriptorPool();
    void createDescriptorSetsLayout();
    void createDescriptorSets();
    void updateResultImageDescriptorSets();
    void createUniformBuffers();
    void createRTPipeline();
    void createPostprocessPipeline();
    void createPredictDenoisePipeline();
    void createAutoExposurePipeline();
};

#endif // MANUEME_DENOISER_H
