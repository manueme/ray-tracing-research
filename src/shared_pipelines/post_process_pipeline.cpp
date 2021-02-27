/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "post_process_pipeline.h"
#include "core/buffer.h"
#include "core/device.h"
#include "core/texture.h"
#include <array>

BasePostProcessPipeline::BasePostProcessPipeline(Device* t_vulkanDevice)
    : m_device(t_vulkanDevice->logicalDevice)
    , m_vulkanDevice(t_vulkanDevice)
{
}

BasePostProcessPipeline::~BasePostProcessPipeline()
{
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set0Scene, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set1InputColor, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set2Exposure, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set3ResultImage, nullptr);
}

void BasePostProcessPipeline::buildCommandBuffer(
    VkCommandBuffer t_commandBuffer, uint32_t t_width, uint32_t t_height)
{
    buildCommandBuffer(0, t_commandBuffer, t_width, t_height);
}

void BasePostProcessPipeline::buildCommandBuffer(
    uint32_t t_commandIndex, VkCommandBuffer t_commandBuffer, uint32_t t_width, uint32_t t_height)
{
    vkCmdBindPipeline(t_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    std::vector<VkDescriptorSet> trainComputeDescriptorSets
        = { m_descriptorSets.set0Scene[t_commandIndex],
              m_descriptorSets.set1InputColor[t_commandIndex],
              m_descriptorSets.set2Exposure[t_commandIndex],
              m_descriptorSets.set3ResultImage[t_commandIndex] };
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

void BasePostProcessPipeline::createPipeline(
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
void BasePostProcessPipeline::createDescriptorSets(
    VkDescriptorPool t_descriptorPool, Buffer* t_sceneBuffer, Buffer* t_exposureBuffer)
{
    std::vector<Buffer> sceneBuffers = { *t_sceneBuffer };
    std::vector<Buffer> exposureBuffers = { *t_exposureBuffer };
    createDescriptorSets(t_descriptorPool, sceneBuffers, 1, exposureBuffers, 1);
}

void BasePostProcessPipeline::createDescriptorSets(VkDescriptorPool t_descriptorPool,
    std::vector<Buffer>& t_sceneBuffers, uint32_t t_inputColorDescriptorCount,
    std::vector<Buffer>& t_exposureBuffers, uint32_t t_outputColorDescriptorCount)

{
    // Set 0: Scene descriptor
    auto layoutCount = t_sceneBuffers.size();
    std::vector<VkDescriptorSetLayout> sceneLayouts(layoutCount, m_descriptorSetLayouts.set0Scene);
    VkDescriptorSetAllocateInfo set0AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            sceneLayouts.data(),
            layoutCount);
    m_descriptorSets.set0Scene.resize(layoutCount);
    CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &set0AllocInfo, m_descriptorSets.set0Scene.data()))
    for (size_t i = 0; i < layoutCount; i++) {
        std::vector<VkWriteDescriptorSet> writeDescriptorSet0 = {
            // Binding 0:
            initializers::writeDescriptorSet(m_descriptorSets.set0Scene[i],
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                0,
                &t_sceneBuffers[i].descriptor),
        };
        vkUpdateDescriptorSets(m_device,
            writeDescriptorSet0.size(),
            writeDescriptorSet0.data(),
            0,
            VK_NULL_HANDLE);
    }

    // Set 1: Input color
    layoutCount = t_inputColorDescriptorCount;
    std::vector<VkDescriptorSetLayout> inputColorLayouts(layoutCount,
        m_descriptorSetLayouts.set1InputColor);
    VkDescriptorSetAllocateInfo set1AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            inputColorLayouts.data(),
            layoutCount);
    m_descriptorSets.set1InputColor.resize(layoutCount);
    CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &set1AllocInfo, m_descriptorSets.set1InputColor.data()))

    // Set 2: Exposure descriptor
    layoutCount = t_exposureBuffers.size();
    std::vector<VkDescriptorSetLayout> exposureLayouts(layoutCount,
        m_descriptorSetLayouts.set2Exposure);
    VkDescriptorSetAllocateInfo set2AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            exposureLayouts.data(),
            layoutCount);
    m_descriptorSets.set2Exposure.resize(layoutCount);
    CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &set2AllocInfo, m_descriptorSets.set2Exposure.data()))
    for (size_t i = 0; i < layoutCount; i++) {
        std::vector<VkWriteDescriptorSet> writeDescriptorSet2 = {
            // Binding 0:
            initializers::writeDescriptorSet(m_descriptorSets.set2Exposure[i],
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                0,
                &t_exposureBuffers[i].descriptor),
        };
        vkUpdateDescriptorSets(m_device,
            writeDescriptorSet2.size(),
            writeDescriptorSet2.data(),
            0,
            VK_NULL_HANDLE);
    }

    // Set 3: Result image descriptor
    layoutCount = t_outputColorDescriptorCount;
    std::vector<VkDescriptorSetLayout> outputColorLayouts(layoutCount,
        m_descriptorSetLayouts.set3ResultImage);
    VkDescriptorSetAllocateInfo set3AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            outputColorLayouts.data(),
            layoutCount);
    m_descriptorSets.set3ResultImage.resize(layoutCount);
    CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &set3AllocInfo, m_descriptorSets.set3ResultImage.data()))
}

PostProcessPipeline::PostProcessPipeline(Device* t_vulkanDevice)
    : BasePostProcessPipeline(t_vulkanDevice)
{
}

PostProcessPipeline::~PostProcessPipeline() { }

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

    // Set 1: Input color
    setLayoutBindings.clear();
    setLayoutBindings.push_back(
        // Binding 0 : Input Image Color
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0));
    descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
        setLayoutBindings.size());
    CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_descriptorSetLayouts.set1InputColor))

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

    // Set 3: Result Image
    setLayoutBindings.clear();
    setLayoutBindings.push_back(
        // Binding 0 : Result Image Color
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0));
    descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
        setLayoutBindings.size());
    CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_descriptorSetLayouts.set3ResultImage))

    std::array<VkDescriptorSetLayout, 4> setLayouts = { m_descriptorSetLayouts.set0Scene,
        m_descriptorSetLayouts.set1InputColor,
        m_descriptorSetLayouts.set2Exposure,
        m_descriptorSetLayouts.set3ResultImage };
    VkPipelineLayoutCreateInfo postprocessPipelineLayoutCreateInfo
        = initializers::pipelineLayoutCreateInfo(setLayouts.data(), setLayouts.size());
    CHECK_RESULT(vkCreatePipelineLayout(m_device,
        &postprocessPipelineLayoutCreateInfo,
        nullptr,
        &m_pipelineLayout))
}

void PostProcessPipeline::updateResultImageDescriptorSets(
    Texture* t_inputColor, Texture* t_outputColor)
{
    updateResultImageDescriptorSets(0, t_inputColor, t_outputColor);
}

void PostProcessPipeline::updateResultImageDescriptorSets(
    uint32_t t_descriptorIndex, Texture* t_inputColor, Texture* t_outputColor)
{
    VkWriteDescriptorSet inputImageWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set1InputColor[t_descriptorIndex],
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            0,
            &t_inputColor->descriptor);

    VkWriteDescriptorSet outputImageWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set3ResultImage[t_descriptorIndex],
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            0,
            &t_outputColor->descriptor);

    std::vector<VkWriteDescriptorSet> writeDescriptorSet1AutoExposure
        = { inputImageWrite, outputImageWrite };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet1AutoExposure.size()),
        writeDescriptorSet1AutoExposure.data(),
        0,
        VK_NULL_HANDLE);
}

PostProcessWithBuffersPipeline::PostProcessWithBuffersPipeline(Device* t_vulkanDevice)
    : BasePostProcessPipeline(t_vulkanDevice)
{
}

PostProcessWithBuffersPipeline::~PostProcessWithBuffersPipeline() { }

void PostProcessWithBuffersPipeline::createDescriptorSetsLayout()
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

    // Set 1 Input image buffer
    setLayoutBindings.clear();
    setLayoutBindings.push_back(
        // Binding 0 : Input image
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0));
    descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
        setLayoutBindings.size());
    CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_descriptorSetLayouts.set1InputColor));

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

    // Set 3: Output Image
    setLayoutBindings.clear();
    setLayoutBindings.push_back(
        // Binding 0 : Result Image Color
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0));
    descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
        setLayoutBindings.size());
    CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_descriptorSetLayouts.set3ResultImage))

    std::array<VkDescriptorSetLayout, 4> setLayouts = { m_descriptorSetLayouts.set0Scene,
        m_descriptorSetLayouts.set1InputColor,
        m_descriptorSetLayouts.set2Exposure,
        m_descriptorSetLayouts.set3ResultImage };
    VkPipelineLayoutCreateInfo postprocessPipelineLayoutCreateInfo
        = initializers::pipelineLayoutCreateInfo(setLayouts.data(), setLayouts.size());
    CHECK_RESULT(vkCreatePipelineLayout(m_device,
        &postprocessPipelineLayoutCreateInfo,
        nullptr,
        &m_pipelineLayout))
}

void PostProcessWithBuffersPipeline::updateResultImageDescriptorSets(
    Buffer* t_inputColorBuffer, Texture* t_outputColor)
{
    updateResultImageDescriptorSets(0, t_inputColorBuffer, t_outputColor);
}

void PostProcessWithBuffersPipeline::updateResultImageDescriptorSets(
    uint32_t t_descriptorIndex, Buffer* t_inputColorBuffer, Texture* t_outputColor)
{
    VkWriteDescriptorSet inputImageBufferWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set1InputColor[t_descriptorIndex],
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            0,
            &t_inputColorBuffer->descriptor);
    std::vector<VkWriteDescriptorSet> writeDescriptorSet1 = { inputImageBufferWrite };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet1.size()),
        writeDescriptorSet1.data(),
        0,
        VK_NULL_HANDLE);

    VkWriteDescriptorSet outputImageWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set3ResultImage[t_descriptorIndex],
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            0,
            &t_outputColor->descriptor);
    std::vector<VkWriteDescriptorSet> writeDescriptorSet3AutoExposure = { outputImageWrite };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet3AutoExposure.size()),
        writeDescriptorSet3AutoExposure.data(),
        0,
        VK_NULL_HANDLE);
}
