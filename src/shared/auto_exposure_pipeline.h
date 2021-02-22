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

class AutoExposurePipeline {
public:
    AutoExposurePipeline(Device* t_vulkanDevice);

    ~AutoExposurePipeline();

    void buildCommandBuffer(VkCommandBuffer t_commandBuffer);

    void createDescriptorSetsLayout();

    void createPipeline(
        VkPipelineCache t_pipelineCache, VkPipelineShaderStageCreateInfo t_shaderStage);

    void createDescriptorSets(VkDescriptorPool t_descriptorPool, Buffer* t_exposureBuffer);

    void updateResultImageDescriptorSets(Texture* t_result);

private:
    Device* m_vulkanDevice;
    VkDevice m_device;

    VkPipeline m_pipeline;
    VkPipelineLayout m_pipelineLayout;

    struct {
        VkDescriptorSet set0InputImage;
        VkDescriptorSet set1Exposure;
    } m_descriptorSets;
    struct {
        VkDescriptorSetLayout set0InputImage;
        VkDescriptorSetLayout set1Exposure;
    } m_descriptorSetLayouts;
};

#endif // SHARED_AUTO_EXPOSURE_PIPELINE_H
