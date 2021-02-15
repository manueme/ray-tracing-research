/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "auto_exposure_pipeline.h"
#include "core/buffer.h"
#include "core/device.h"
#include "core/texture.h"
#include <array>

AutoExposurePipeline::AutoExposurePipeline(Device* t_vulkanDevice)
    : m_device(t_vulkanDevice->logicalDevice)
    , m_vulkanDevice(t_vulkanDevice)
{
}

void AutoExposurePipeline::buildCommandBuffer(VkCommandBuffer t_commandBuffer)
{
    vkCmdBindPipeline(t_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    std::vector<VkDescriptorSet> trainComputeDescriptorSets
        = { m_descriptorSets.set0InputImage, m_descriptorSets.set1Exposure };
    vkCmdBindDescriptorSets(t_commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_pipelineLayout,
        0,
        trainComputeDescriptorSets.size(),
        trainComputeDescriptorSets.data(),
        0,
        nullptr);
    vkCmdDispatch(t_commandBuffer, 1, 1, 1);
}

void AutoExposurePipeline::createDescriptorSetsLayout()
{
    // Set 0: Input Image
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings
        = { // Binding 0 : Result Image Color
              initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                  VK_SHADER_STAGE_COMPUTE_BIT,
                  0)
          };
    VkDescriptorSetLayoutCreateInfo descriptorLayout
        = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
            setLayoutBindings.size());
    CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_descriptorSetLayouts.set0InputImage))

    // Set 1 Exposure buffer
    setLayoutBindings.clear();
    setLayoutBindings.push_back(
        // Binding 0 : Scene uniform buffer
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0));
    descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
        setLayoutBindings.size());
    CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_descriptorSetLayouts.set1Exposure));

    std::array<VkDescriptorSetLayout, 2> setLayouts
        = { m_descriptorSetLayouts.set0InputImage, m_descriptorSetLayouts.set1Exposure };
    VkPipelineLayoutCreateInfo postprocessPipelineLayoutCreateInfo
        = initializers::pipelineLayoutCreateInfo(setLayouts.data(), setLayouts.size());
    CHECK_RESULT(vkCreatePipelineLayout(m_device,
        &postprocessPipelineLayoutCreateInfo,
        nullptr,
        &m_pipelineLayout))
}

void AutoExposurePipeline::createPipeline(
    VkPipelineCache t_pipelineCache, VkPipelineShaderStageCreateInfo t_shaderStage)
{
    VkComputePipelineCreateInfo computePipelineCreateInfo {};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.layout = m_pipelineLayout;
    computePipelineCreateInfo.flags = 0;
    computePipelineCreateInfo.stage = t_shaderStage;
    CHECK_RESULT(vkCreateComputePipelines(m_device,
        t_pipelineCache,
        1,
        &computePipelineCreateInfo,
        nullptr,
        &m_pipeline))
}

void AutoExposurePipeline::createDescriptorSets(
    VkDescriptorPool t_descriptorPool, Buffer* t_exposureBuffer)
{
    // Set 0: Result image descriptor
    VkDescriptorSetAllocateInfo set0AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            &m_descriptorSetLayouts.set0InputImage,
            1);
    CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &set0AllocInfo, &m_descriptorSets.set0InputImage))

    // Set 1: Exposure descriptor
    VkDescriptorSetAllocateInfo set1AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            &m_descriptorSetLayouts.set1Exposure,
            1);
    CHECK_RESULT(vkAllocateDescriptorSets(m_device, &set1AllocInfo, &m_descriptorSets.set1Exposure))
    VkWriteDescriptorSet uniformBufferWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set1Exposure,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            0,
            &t_exposureBuffer->descriptor);
    std::vector<VkWriteDescriptorSet> writeDescriptorSet1 = { uniformBufferWrite };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet1.size()),
        writeDescriptorSet1.data(),
        0,
        VK_NULL_HANDLE);
}

void AutoExposurePipeline::updateResultImageDescriptorSets(Texture* t_result)
{
    VkWriteDescriptorSet inputImageWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set0InputImage,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            0,
            &t_result->descriptor);
    std::vector<VkWriteDescriptorSet> writeDescriptorSet0AutoExposure = { inputImageWrite };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet0AutoExposure.size()),
        writeDescriptorSet0AutoExposure.data(),
        0,
        VK_NULL_HANDLE);
}

AutoExposurePipeline::~AutoExposurePipeline() {
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device,
                                 m_descriptorSetLayouts.set0InputImage, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set1Exposure, nullptr);
}
