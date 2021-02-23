/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef SHARED_AUTO_EXPOSURE_PIPELINE_H
#define SHARED_AUTO_EXPOSURE_PIPELINE_H

#include "vulkan/vulkan_core.h"

class Device;
class Buffer;
class Texture;

class BaseAutoExposurePipeline {
public:
    void buildCommandBuffer(VkCommandBuffer t_commandBuffer);

    void createPipeline(
        VkPipelineCache t_pipelineCache, VkPipelineShaderStageCreateInfo t_shaderStage);

    void createDescriptorSets(VkDescriptorPool t_descriptorPool, Buffer* t_exposureBuffer);

protected:
    BaseAutoExposurePipeline(Device* t_vulkanDevice);

    ~BaseAutoExposurePipeline();

    virtual void createDescriptorSetsLayout() = 0;

    Device* m_vulkanDevice;
    VkDevice m_device;

    VkPipeline m_pipeline;
    VkPipelineLayout m_pipelineLayout;

    struct {
        VkDescriptorSet set0InputColor;
        VkDescriptorSet set1Exposure;
    } m_descriptorSets;
    struct {
        VkDescriptorSetLayout set0InputColor;
        VkDescriptorSetLayout set1Exposure;
    } m_descriptorSetLayouts;
};

class AutoExposurePipeline : public BaseAutoExposurePipeline {
public:
    AutoExposurePipeline(Device* t_vulkanDevice);

    ~AutoExposurePipeline();

    void createDescriptorSetsLayout() override;

    void updateResultImageDescriptorSets(Texture* t_result);
};

class AutoExposureWithBuffersPipeline : public BaseAutoExposurePipeline {
public:
    AutoExposureWithBuffersPipeline(Device* t_vulkanDevice);

    ~AutoExposureWithBuffersPipeline();

    void createDescriptorSetsLayout() override;

    void updateResultImageDescriptorSets(Buffer* t_inputImageBuffer);
};

#endif // SHARED_AUTO_EXPOSURE_PIPELINE_H
