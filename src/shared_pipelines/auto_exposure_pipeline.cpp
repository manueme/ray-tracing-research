/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "auto_exposure_pipeline.h"
#include "core/buffer.h"
#include "core/device.h"
#include "core/texture.h"
#include <array>

BaseAutoExposurePipeline::BaseAutoExposurePipeline(Device* t_vulkanDevice)
    : m_device(t_vulkanDevice->logicalDevice)
    , m_vulkanDevice(t_vulkanDevice)
{
}

BaseAutoExposurePipeline::~BaseAutoExposurePipeline()
{
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set0InputColor, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set1Exposure, nullptr);
}

void BaseAutoExposurePipeline::buildCommandBuffer(VkCommandBuffer t_commandBuffer)
{
    buildCommandBuffer(0, t_commandBuffer);
}

void BaseAutoExposurePipeline::buildCommandBuffer(
    uint32_t t_commandIndex, VkCommandBuffer t_commandBuffer)
{
    vkCmdBindPipeline(t_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    std::vector<VkDescriptorSet> descriptorSets = { m_descriptorSets.set0InputColor[t_commandIndex],
        m_descriptorSets.set1Exposure[t_commandIndex] };
    vkCmdBindDescriptorSets(t_commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_pipelineLayout,
        0,
        descriptorSets.size(),
        descriptorSets.data(),
        0,
        nullptr);
    vkCmdDispatch(t_commandBuffer, 1, 1, 1);
}

void BaseAutoExposurePipeline::createPipeline(
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

void BaseAutoExposurePipeline::createDescriptorSets(
    VkDescriptorPool t_descriptorPool, Buffer* t_exposureBuffer)
{
    std::vector<Buffer> exposureBuffers = { *t_exposureBuffer };
    createDescriptorSets(t_descriptorPool, exposureBuffers, 1);
}

void BaseAutoExposurePipeline::createDescriptorSets(VkDescriptorPool t_descriptorPool,
    std::vector<Buffer>& t_exposureBuffers, uint32_t t_inputColorDescriptorCount)
{
    // Set 0: Result image descriptor
    auto layoutCount = t_inputColorDescriptorCount;
    std::vector<VkDescriptorSetLayout> inputColorLayouts(layoutCount,
        m_descriptorSetLayouts.set0InputColor);
    VkDescriptorSetAllocateInfo set0AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            inputColorLayouts.data(),
            t_inputColorDescriptorCount);
    m_descriptorSets.set0InputColor.resize(layoutCount);
    CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &set0AllocInfo, m_descriptorSets.set0InputColor.data()))

    // Set 1: Exposure descriptor
    layoutCount = t_exposureBuffers.size();
    std::vector<VkDescriptorSetLayout> exposureLayouts(layoutCount,
        m_descriptorSetLayouts.set1Exposure);
    VkDescriptorSetAllocateInfo set1AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            exposureLayouts.data(),
            layoutCount);
    m_descriptorSets.set1Exposure.resize(layoutCount);
    CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &set1AllocInfo, m_descriptorSets.set1Exposure.data()))
    for (size_t i = 0; i < layoutCount; i++) {
        std::vector<VkWriteDescriptorSet> writeDescriptorSet1 = {
            // Binding 0:
            initializers::writeDescriptorSet(m_descriptorSets.set1Exposure[i],
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                0,
                &t_exposureBuffers[i].descriptor),
        };
        vkUpdateDescriptorSets(m_device,
            writeDescriptorSet1.size(),
            writeDescriptorSet1.data(),
            0,
            VK_NULL_HANDLE);
    }
}

AutoExposurePipeline::AutoExposurePipeline(Device* t_vulkanDevice)
    : BaseAutoExposurePipeline(t_vulkanDevice)
{
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
        &m_descriptorSetLayouts.set0InputColor))

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
        = { m_descriptorSetLayouts.set0InputColor, m_descriptorSetLayouts.set1Exposure };
    VkPipelineLayoutCreateInfo postprocessPipelineLayoutCreateInfo
        = initializers::pipelineLayoutCreateInfo(setLayouts.data(), setLayouts.size());
    CHECK_RESULT(vkCreatePipelineLayout(m_device,
        &postprocessPipelineLayoutCreateInfo,
        nullptr,
        &m_pipelineLayout))
}

void AutoExposurePipeline::updateResultImageDescriptorSets(Texture* t_result)
{
    updateResultImageDescriptorSets(0, t_result);
}

void AutoExposurePipeline::updateResultImageDescriptorSets(
    uint32_t t_descriptorIndex, Texture* t_result)
{
    VkWriteDescriptorSet inputImageWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set0InputColor[t_descriptorIndex],
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

AutoExposurePipeline::~AutoExposurePipeline() { }

AutoExposureWithBuffersPipeline::AutoExposureWithBuffersPipeline(Device* t_vulkanDevice)
    : BaseAutoExposurePipeline(t_vulkanDevice)
{
}

AutoExposureWithBuffersPipeline::~AutoExposureWithBuffersPipeline() { }

void AutoExposureWithBuffersPipeline::createDescriptorSetsLayout()
{
    // Set 0: Input Buffer
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = { // Binding 0 : Input Image Color
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0)
    };
    VkDescriptorSetLayoutCreateInfo descriptorLayout
        = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
            setLayoutBindings.size());
    CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_descriptorSetLayouts.set0InputColor))

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
        = { m_descriptorSetLayouts.set0InputColor, m_descriptorSetLayouts.set1Exposure };
    VkPipelineLayoutCreateInfo postprocessPipelineLayoutCreateInfo
        = initializers::pipelineLayoutCreateInfo(setLayouts.data(), setLayouts.size());
    CHECK_RESULT(vkCreatePipelineLayout(m_device,
        &postprocessPipelineLayoutCreateInfo,
        nullptr,
        &m_pipelineLayout))
}

void AutoExposureWithBuffersPipeline::updateResultImageDescriptorSets(Buffer* t_inputImageBuffer)
{
    updateResultImageDescriptorSets(0, t_inputImageBuffer);
}

void AutoExposureWithBuffersPipeline::updateResultImageDescriptorSets(
    uint32_t t_descriptorIndex, Buffer* t_inputImageBuffer)
{
    VkWriteDescriptorSet inputBufferWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set0InputColor[t_descriptorIndex],
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            0,
            &t_inputImageBuffer->descriptor);
    std::vector<VkWriteDescriptorSet> writeDescriptorSet0 = { inputBufferWrite };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet0.size()),
        writeDescriptorSet0.data(),
        0,
        VK_NULL_HANDLE);
}
