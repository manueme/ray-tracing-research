/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef COMMON_POST_PROCESS_PIPELINE_H
#define COMMON_POST_PROCESS_PIPELINE_H

#include "vulkan/vulkan_core.h"

class Device;
class Buffer;
class Texture;

class PostProcessPipeline {
public:
    PostProcessPipeline(Device* t_vulkanDevice);

    ~PostProcessPipeline();

    void buildCommandBuffer(VkCommandBuffer t_commandBuffer, uint32_t t_width, uint32_t t_height);

    void createDescriptorSetsLayout();

    void createPipeline(
        VkPipelineCache t_pipelineCache, VkPipelineShaderStageCreateInfo t_shaderStage);

    void createDescriptorSets(
        VkDescriptorPool t_descriptorPool, Buffer* t_sceneBuffer, Buffer* t_exposureBuffer);

    void updateResultImageDescriptorSets(Texture* t_resultInput, Texture* t_resultOutput);

private:
    Device* m_vulkanDevice;
    VkDevice m_device;

    VkPipeline m_pipeline;
    VkPipelineLayout m_pipelineLayout;

    struct {
        VkDescriptorSet set0Scene;
        VkDescriptorSet set1InputImage;
        VkDescriptorSet set2Exposure;
    } m_descriptorSets;
    struct {
        VkDescriptorSetLayout set0Scene;
        VkDescriptorSetLayout set1InputImage;
        VkDescriptorSetLayout set2Exposure;
    } m_descriptorSetLayouts;
};

#endif // COMMON_POST_PROCESS_PIPELINE_H
