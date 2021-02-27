/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef SHARED_POST_PROCESS_PIPELINE_H
#define SHARED_POST_PROCESS_PIPELINE_H

#include "vulkan/vulkan_core.h"
#include <vector>

class Device;
class Buffer;
class Texture;

class BasePostProcessPipeline {
public:
    void buildCommandBuffer(VkCommandBuffer t_commandBuffer, uint32_t t_width, uint32_t t_height);

    void buildCommandBuffer(uint32_t t_commandIndex, VkCommandBuffer t_commandBuffer,
        uint32_t t_width, uint32_t t_height);

    void createPipeline(
        VkPipelineCache t_pipelineCache, VkPipelineShaderStageCreateInfo t_shaderStage);

    void createDescriptorSets(
        VkDescriptorPool t_descriptorPool, Buffer* t_sceneBuffer, Buffer* t_exposureBuffer);

    void createDescriptorSets(VkDescriptorPool t_descriptorPool,
        std::vector<Buffer>& t_sceneBuffers, uint32_t t_inputColorDescriptorCount,
        std::vector<Buffer>& t_exposureBuffers, uint32_t t_outputColorDescriptorCount);

protected:
    BasePostProcessPipeline(Device* t_vulkanDevice);

    ~BasePostProcessPipeline();

    virtual void createDescriptorSetsLayout() = 0;

    Device* m_vulkanDevice;
    VkDevice m_device;

    VkPipeline m_pipeline;
    VkPipelineLayout m_pipelineLayout;

    struct {
        std::vector<VkDescriptorSet> set0Scene;
        std::vector<VkDescriptorSet> set1InputColor;
        std::vector<VkDescriptorSet> set2Exposure;
        std::vector<VkDescriptorSet> set3ResultImage;
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

    void updateResultImageDescriptorSets(
        uint32_t t_descriptorIndex, Texture* t_inputColor, Texture* t_outputColor);
};

class PostProcessWithBuffersPipeline : public BasePostProcessPipeline {
public:
    PostProcessWithBuffersPipeline(Device* t_vulkanDevice);

    ~PostProcessWithBuffersPipeline();

    void createDescriptorSetsLayout() override;

    void updateResultImageDescriptorSets(Buffer* t_inputColorBuffer, Texture* t_outputColor);

    void updateResultImageDescriptorSets(
        uint32_t t_descriptorIndex, Buffer* t_inputColorBuffer, Texture* t_outputColor);
};

#endif // SHARED_POST_PROCESS_PIPELINE_H
