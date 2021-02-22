/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_RAY_TRACING_OPTIX_DENOISER_H
#define MANUEME_RAY_TRACING_OPTIX_DENOISER_H

#include "semaphore_cuda.h"

#include "base_project.h"
#include "core/texture.h"


class RayTracingPipeline;
class AutoExposurePipeline;
class PostProcessPipeline;
class DenoiserOptixPipeline;

class RayTracingOptixDenoiser : public BaseProject {
public:
    RayTracingOptixDenoiser();
    ~RayTracingOptixDenoiser();

private:
    RayTracingPipeline* m_rayTracing;
    AutoExposurePipeline* m_autoExposure;
    PostProcessPipeline* m_postProcess;
    DenoiserOptixPipeline* m_denoiser;

    // Images used to store ray traced image
    struct {
        Texture rtResult;
        Texture denoiseResult;
        Texture postProcessResult;
        Texture depthMap;
        Texture normalMap;
        Texture albedo;
    } m_storageImage;

    Buffer m_instancesBuffer;
    Buffer m_lightsBuffer;
    Buffer m_materialsBuffer;

    struct UniformData {
        glm::mat4 view;
        glm::mat4 projection;
        glm::mat4 viewInverse { glm::mat4(0.0) };
        glm::mat4 projInverse { glm::mat4(0.0) };
        glm::vec4 overrideSunDirection { glm::vec4(0.0) };
        uint32_t frameIteration = 0; // Current frame iteration number
        uint32_t frame = 0; // Current frame
        uint32_t frameChanged = 1; // Current frame changed size
        float manualExposureAdjust = 0.0f;
    } m_sceneUniformData;
    Buffer m_sceneBuffer;

    struct ExposureUniformData {
        float exposure = 1.0f;
    } m_exposureData;
    Buffer m_exposureBuffer;

    // Timeline semaphores for denoiser system synchronization
    SemaphoreCuda m_denoiseWaitFor;
    SemaphoreCuda m_denoiseSignalTo;
    uint64_t m_timelineValue { 0 };
    // ---

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
    void createAutoExposurePipeline();

    VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeature;
    void getEnabledFeatures() override;
};

#endif // MANUEME_RAY_TRACING_OPTIX_DENOISER_H
