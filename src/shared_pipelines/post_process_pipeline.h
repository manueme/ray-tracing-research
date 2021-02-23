/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef SHARED_POST_PROCESS_PIPELINE_H
#define SHARED_POST_PROCESS_PIPELINE_H

#include "vulkan/vulkan_core.h"

class Device;
class Buffer;
class Texture;

class BasePostProcessPipeline {
public:
    void buildCommandBuffer(VkCommandBuffer t_commandBuffer, uint32_t t_width, uint32_t t_height);

    void createPipeline(
        VkPipelineCache t_pipelineCache, VkPipelineShaderStageCreateInfo t_shaderStage);

    void createDescriptorSets(
        VkDescriptorPool t_descriptorPool, Buffer* t_sceneBuffer, Buffer* t_exposureBuffer);

protected:
    BasePostProcessPipeline(Device* t_vulkanDevice);

    ~BasePostProcessPipeline();

    virtual void createDescriptorSetsLayout() = 0;

    Device* m_vulkanDevice;
    VkDevice m_device;

    VkPipeline m_pipeline;
    VkPipelineLayout m_pipelineLayout;

    struct {
        VkDescriptorSet set0Scene;
        VkDescriptorSet set1InputColor;
        VkDescriptorSet set2Exposure;
        VkDescriptorSet set3ResultImage;
    } m_descriptorSets;
    struct {
        VkDescriptorSetLayout set0Scene;
        VkDescriptorSetLayout set1InputColor;
        VkDescriptorSetLayout set2Exposure;
        VkDescriptorSetLayout set3ResultImage;
    } m_descriptorSetLayouts;
};

class PostProcessPipeline : public BasePostProcessPipeline {
public:
    PostProcessPipeline(Device* t_vulkanDevice);

    ~PostProcessPipeline();

    void createDescriptorSetsLayout() override;

    void updateResultImageDescriptorSets(Texture* t_inputColor, Texture* t_outputColor);
};

class PostProcessWithBuffersPipeline : public BasePostProcessPipeline {
public:
    PostProcessWithBuffersPipeline(Device* t_vulkanDevice);

    ~PostProcessWithBuffersPipeline();

    void createDescriptorSetsLayout() override;

    void updateResultImageDescriptorSets(Buffer* t_inputColorBuffer, Texture* t_outputColor);
};

#endif // SHARED_POST_PROCESS_PIPELINE_H
