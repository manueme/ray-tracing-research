/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "post_process_pipeline.h"
#include "core/buffer.h"
#include "core/device.h"
#include "core/texture.h"
#include <array>

PostProcessPipeline::PostProcessPipeline(Device* t_vulkanDevice)
    : m_device(t_vulkanDevice->logicalDevice)
    , m_vulkanDevice(t_vulkanDevice)
{
}

PostProcessPipeline::~PostProcessPipeline()
{
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set0Scene, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set1InputImage, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set2Exposure, nullptr);
}

void PostProcessPipeline::buildCommandBuffer(
    VkCommandBuffer t_commandBuffer, uint32_t t_width, uint32_t t_height)
{
    vkCmdBindPipeline(t_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    std::vector<VkDescriptorSet> trainComputeDescriptorSets = { m_descriptorSets.set0Scene,
        m_descriptorSets.set1InputImage,
        m_descriptorSets.set2Exposure };
    vkCmdBindDescriptorSets(t_commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_pipelineLayout,
        0,
        trainComputeDescriptorSets.size(),
        trainComputeDescriptorSets.data(),
        0,
        nullptr);
    uint32_t ceilWidth = glm::ceil(t_width / 16.0f) * 16;
    uint32_t ceilHeight = glm::ceil(t_height / 16.0f) * 16;
    vkCmdDispatch(t_commandBuffer, ceilWidth, ceilHeight, 1);
}

void PostProcessPipeline::createDescriptorSetsLayout()
{
    // Set 0 Scene buffer
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings
        = { // Binding 0 : Scene uniform buffer
              initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                  VK_SHADER_STAGE_COMPUTE_BIT,
                  0)
          };
    VkDescriptorSetLayoutCreateInfo descriptorLayout
        = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
            setLayoutBindings.size());
    CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_descriptorSetLayouts.set0Scene));

    // Set 1: Input Image
    setLayoutBindings.clear();
    setLayoutBindings.push_back(
        // Binding 0 : Input Image Color
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0));
    setLayoutBindings.push_back(
        // Binding 1 : Result Image Color
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_SHADER_STAGE_COMPUTE_BIT,
            1));
    descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
        setLayoutBindings.size());
    CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_descriptorSetLayouts.set1InputImage))

    // Set 2 Exposure buffer
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
        &m_descriptorSetLayouts.set2Exposure));

    std::array<VkDescriptorSetLayout, 3> setLayouts = { m_descriptorSetLayouts.set0Scene,
        m_descriptorSetLayouts.set1InputImage,
        m_descriptorSetLayouts.set2Exposure };
    VkPipelineLayoutCreateInfo postprocessPipelineLayoutCreateInfo
        = initializers::pipelineLayoutCreateInfo(setLayouts.data(), setLayouts.size());
    CHECK_RESULT(vkCreatePipelineLayout(m_device,
        &postprocessPipelineLayoutCreateInfo,
        nullptr,
        &m_pipelineLayout))
}

void PostProcessPipeline::createPipeline(
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

void PostProcessPipeline::createDescriptorSets(
    VkDescriptorPool t_descriptorPool, Buffer* t_sceneBuffer, Buffer* t_exposureBuffer)
{
    // Set 0: Scene descriptor
    VkDescriptorSetAllocateInfo set0AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            &m_descriptorSetLayouts.set0Scene,
            1);
    CHECK_RESULT(vkAllocateDescriptorSets(m_device, &set0AllocInfo, &m_descriptorSets.set0Scene))
    VkWriteDescriptorSet uniformBufferWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set0Scene,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            0,
            &t_sceneBuffer->descriptor);
    std::vector<VkWriteDescriptorSet> writeDescriptorSet0 = { uniformBufferWrite };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet0.size()),
        writeDescriptorSet0.data(),
        0,
        VK_NULL_HANDLE);

    // Set 1: Result image descriptor
    VkDescriptorSetAllocateInfo set1AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            &m_descriptorSetLayouts.set1InputImage,
            1);
    CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &set1AllocInfo, &m_descriptorSets.set1InputImage))

    // Set 2: Exposure descriptor
    VkDescriptorSetAllocateInfo set2AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            &m_descriptorSetLayouts.set2Exposure,
            1);
    CHECK_RESULT(vkAllocateDescriptorSets(m_device, &set2AllocInfo, &m_descriptorSets.set2Exposure))
    uniformBufferWrite = initializers::writeDescriptorSet(m_descriptorSets.set2Exposure,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        0,
        &t_exposureBuffer->descriptor);
    std::vector<VkWriteDescriptorSet> writeDescriptorSet2 = { uniformBufferWrite };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet2.size()),
        writeDescriptorSet2.data(),
        0,
        VK_NULL_HANDLE);
}

void PostProcessPipeline::updateResultImageDescriptorSets(
    Texture* t_resultInput, Texture* t_resultOutput)
{
    VkWriteDescriptorSet inputImageWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set1InputImage,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            0,
            &t_resultInput->descriptor);
    VkWriteDescriptorSet outputImageWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set1InputImage,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            1,
            &t_resultOutput->descriptor);
    std::vector<VkWriteDescriptorSet> writeDescriptorSet1AutoExposure
        = { inputImageWrite, outputImageWrite };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet1AutoExposure.size()),
        writeDescriptorSet1AutoExposure.data(),
        0,
        VK_NULL_HANDLE);
}
