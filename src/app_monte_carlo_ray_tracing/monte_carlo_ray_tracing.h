/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_MONTE_CARLO_RAY_TRACING_H
#define MANUEME_MONTE_CARLO_RAY_TRACING_H

#include "base_project.h"
#include "core/texture.h"

class MCRayTracingPipeline;
class AutoExposurePipeline;
class PostProcessPipeline;

class MonteCarloRTApp : public BaseProject {
public:
    MonteCarloRTApp();
    ~MonteCarloRTApp();

private:
    MCRayTracingPipeline* m_rayTracing;
    AutoExposurePipeline* m_autoExposure;
    PostProcessPipeline* m_postProcess;


    // Images used to store ray traced image
    struct {
        Texture result;
        Texture postProcessResult;
        // - Normal map, Depth Map and Albedo are used by the denoiser in the denoiser app,
        // can be removed if you don't need them.
        // - The depth map is also used for the DOF effect
        Texture depthMap;
        Texture normalMap;
        Texture albedo;
    } m_storageImage;

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
        float manualExposureAdjust = { 0.0f };
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
    void createAutoExposurePipeline();
};

#endif // MANUEME_MONTE_CARLO_RAY_TRACING_H
