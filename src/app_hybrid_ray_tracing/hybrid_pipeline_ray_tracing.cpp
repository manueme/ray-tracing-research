/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "hybrid_pipeline_ray_tracing.h"
#include "auto_exposure_pipeline.h"
#include "constants.h"
#include "pipelines/hy_ray_tracing_pipeline.h"
#include "post_process_pipeline.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

HybridPipelineRT::HybridPipelineRT()
    : BaseProject("Hybrid Pipeline Ray Tracing", "Hybrid Pipeline Ray Tracing", true)
{
    m_settings.vsync = false;
    m_settings.useRayTracing = true;
    m_settings.useCompute = true;
}

void HybridPipelineRT::render()
{
    if (!m_prepared) {
        return;
    }

    const auto imageIndex = BaseProject::acquireNextImage();

    updateUniformBuffers(imageIndex);

    // Get the frame index that was used for acquisition (needed for the acquisition semaphore)
    size_t frameIndex = m_imageToFrameIndex[imageIndex];

    // Submit the draw command buffer
    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[frameIndex];
    VkPipelineStageFlags drawWaitStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    submitInfo.pWaitDstStageMask = &drawWaitStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_drawCmdBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_compute.semaphores[imageIndex];
    vkResetFences(m_device, 1, &m_inFlightFences[imageIndex]);
    CHECK_RESULT(vkQueueSubmit(m_queue, 1, &submitInfo, m_inFlightFences[imageIndex]))
    // ----

    // Submit Compute Command Buffer:
    vkWaitForFences(m_device, 1, &m_compute.fences[imageIndex], VK_TRUE, UINT64_MAX);
    VkSubmitInfo computeSubmitInfo {};
    computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    computeSubmitInfo.commandBufferCount = 1;
    computeSubmitInfo.pCommandBuffers = &m_compute.commandBuffers[imageIndex];
    computeSubmitInfo.waitSemaphoreCount = 1;
    computeSubmitInfo.pWaitSemaphores = &m_compute.semaphores[imageIndex];
    computeSubmitInfo.signalSemaphoreCount = 1;
    computeSubmitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[imageIndex];
    VkPipelineStageFlags computeWaitStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    computeSubmitInfo.pWaitDstStageMask = &computeWaitStageMask;
    vkResetFences(m_device, 1, &m_compute.fences[imageIndex]);
    CHECK_RESULT(
        vkQueueSubmit(m_compute.queue, 1, &computeSubmitInfo, m_compute.fences[imageIndex]));
    // ----

    if (BaseProject::queuePresentSwapChain(imageIndex) == VK_SUCCESS) {
        ++m_sceneUniformData.frame;
        if (m_sceneUniformData.frame > 6000) {
            m_sceneUniformData.frame = 0;
        }
        std::cout << '\r' << "FPS: " << m_lastFps << "  " << std::flush;
    }
}

void HybridPipelineRT::buildCommandBuffers()
{
    // Draw command buffers
    VkCommandBufferBeginInfo drawCmdBufInfo = {};
    drawCmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    std::array<VkClearValue, 5> rasterClearValues = {};
    rasterClearValues[0].color = m_default_clear_color;
    rasterClearValues[1].color = m_default_clear_color;
    rasterClearValues[2].color = m_default_clear_color;
    rasterClearValues[3].color = m_default_clear_color;
    rasterClearValues[4].depthStencil = { 1.0f, 0 };
    VkRenderPassBeginInfo rasterPassBeginInfo = {};
    rasterPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rasterPassBeginInfo.renderPass = m_offscreenRenderPass;
    rasterPassBeginInfo.renderArea.offset.x = 0;
    rasterPassBeginInfo.renderArea.offset.y = 0;
    rasterPassBeginInfo.renderArea.extent.width = m_width;
    rasterPassBeginInfo.renderArea.extent.height = m_height;
    rasterPassBeginInfo.clearValueCount = static_cast<uint32_t>(rasterClearValues.size());
    rasterPassBeginInfo.pClearValues = rasterClearValues.data();

    for (uint32_t i = 0; i < m_drawCmdBuffers.size(); ++i) {
        rasterPassBeginInfo.framebuffer = m_offscreenFramebuffers[i];

        CHECK_RESULT(vkBeginCommandBuffer(m_drawCmdBuffers[i], &drawCmdBufInfo))

        // Offscreen render pass
        vkCmdBeginRenderPass(m_drawCmdBuffers[i], &rasterPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = initializers::viewport(static_cast<float>(m_width),
            static_cast<float>(m_height),
            0.0f,
            1.0f);
        vkCmdSetViewport(m_drawCmdBuffers[i], 0, 1, &viewport);

        VkRect2D scissor = initializers::rect2D(m_width, m_height, 0, 0);
        vkCmdSetScissor(m_drawCmdBuffers[i], 0, 1, &scissor);

        vkCmdBindPipeline(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.raster);

        std::vector<VkDescriptorSet> descriptorSets = { m_rasterDescriptorSets.set0Scene[i],
            m_rasterDescriptorSets.set1Materials,
            m_rasterDescriptorSets.set2Lights };
        vkCmdBindDescriptorSets(m_drawCmdBuffers[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipelineLayouts.raster,
            0,
            descriptorSets.size(),
            descriptorSets.data(),
            0,
            nullptr);
        m_scene->draw(m_drawCmdBuffers[i], m_pipelineLayouts.raster, vertex_buffer_bind_id);

        vkCmdEndRenderPass(m_drawCmdBuffers[i]);
        // End Offscreen render pass

        // Ray tracing "pass" using the offscreen result (color, depth and normals)
        m_rayTracing->buildCommandBuffer(i, m_drawCmdBuffers[i], m_width, m_height);

        CHECK_RESULT(vkEndCommandBuffer(m_drawCmdBuffers[i]))
    }
    // ---
    // Compute command buffers
    VkCommandBufferBeginInfo computeCmdBufInfo = {};
    computeCmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    for (int32_t i = 0; i < m_compute.commandBuffers.size(); ++i) {
        CHECK_RESULT(vkBeginCommandBuffer(m_compute.commandBuffers[i], &computeCmdBufInfo))
        m_autoExposure->buildCommandBuffer(i, m_compute.commandBuffers[i]);
        m_postProcess->buildCommandBuffer(i, m_compute.commandBuffers[i], m_width, m_height);

        // Move result to swap chain image:

        // Prepare images to transfer
        VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        tools::setImageLayout(m_compute.commandBuffers[i],
            m_swapChain.images[i],
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);
        tools::setImageLayout(m_compute.commandBuffers[i],
            m_storageImages[i].postProcessResultImage.getImage(),
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            subresourceRange);

        VkImageCopy copyRegion {};
        copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copyRegion.srcOffset = { 0, 0, 0 };
        copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copyRegion.dstOffset = { 0, 0, 0 };
        copyRegion.extent = { m_width, m_height, 1 };
        vkCmdCopyImage(m_compute.commandBuffers[i],
            m_storageImages[i].postProcessResultImage.getImage(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            m_swapChain.images[i],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copyRegion);

        // Transition back to previous layouts:
        tools::setImageLayout(m_compute.commandBuffers[i],
            m_swapChain.images[i],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            subresourceRange);
        tools::setImageLayout(m_compute.commandBuffers[i],
            m_storageImages[i].postProcessResultImage.getImage(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL,
            subresourceRange);

        vkEndCommandBuffer(m_compute.commandBuffers[i]);
    }
    // ---
}

void HybridPipelineRT::createDescriptorPool()
{
    // Calculate the number of textures needed
    // Textures are used in TWO descriptor sets: raster (set1Materials) and ray tracing (set3Materials)
    // Each set needs textureCount descriptors, so we need textureCount * 2 total
    uint32_t textureCount = m_scene->textures.empty() ? 1 : static_cast<uint32_t>(m_scene->textures.size());
    uint32_t totalTextureDescriptors = textureCount * 2; // One set for raster, one for ray tracing
    
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
        // Scene uniform buffer
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_swapChain.imageCount },
        // Vertex, Index and Material Indexes
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_swapChain.imageCount },
        // Textures (needs to accommodate textures used in both raster and ray tracing descriptor sets)
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, totalTextureDescriptors },
        // Material array
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
        // Lights array
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
        // Offscreen images (per swapchain image)
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_swapChain.imageCount },
        // Storage images
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_swapChain.imageCount },
    };
    // Calculate max set for pool
    const auto sceneSets = 1 * m_swapChain.imageCount;
    const auto exposurePipelineSets = 2 * m_swapChain.imageCount;
    const auto postProcessPipelineSets = 4 * m_swapChain.imageCount;
    const auto rayTracingPipelineSets = 5 + 3 * m_swapChain.imageCount;
    const auto offscreenPipelineSets = 2 + 1 * m_swapChain.imageCount;
    uint32_t maxSetsForPool = sceneSets + exposurePipelineSets + postProcessPipelineSets
        + rayTracingPipelineSets + offscreenPipelineSets;
    // ---
    VkDescriptorPoolCreateInfo descriptorPoolInfo
        = initializers::descriptorPoolCreateInfo(poolSizes.size(),
            poolSizes.data(),
            maxSetsForPool);
    CHECK_RESULT(vkCreateDescriptorPool(m_device, &descriptorPoolInfo, nullptr, &m_descriptorPool))
}

void HybridPipelineRT::createDescriptorSetLayout()
{
    // Offscreen
    {
        // Set 0 Raster: Scene matrices
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings
            = { // Binding 0 : Vertex shader uniform buffer
                  initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                      0)
              };
        VkDescriptorSetLayoutCreateInfo descriptorLayout
            = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
                setLayoutBindings.size());
        CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
            &descriptorLayout,
            nullptr,
            &m_rasterDescriptorSetLayouts.set0Scene))

        // Set 1 Raster: Material data
        setLayoutBindings.clear();
        // Texture list binding 0
        setLayoutBindings.push_back(
            initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                m_scene->textures.size()));
        // Material list binding 1
        setLayoutBindings.push_back(
            initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                1));
        descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
            setLayoutBindings.size());
        CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
            &descriptorLayout,
            nullptr,
            &m_rasterDescriptorSetLayouts.set1Materials))

        // Set 2 Raster: Lighting data
        setLayoutBindings.clear();
        // Light list binding 0
        setLayoutBindings.push_back(
            initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                1));
        descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
            setLayoutBindings.size());
        CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
            &descriptorLayout,
            nullptr,
            &m_rasterDescriptorSetLayouts.set2Lights))

        std::array<VkDescriptorSetLayout, 3> setLayouts = { m_rasterDescriptorSetLayouts.set0Scene,
            m_rasterDescriptorSetLayouts.set1Materials,
            m_rasterDescriptorSetLayouts.set2Lights };
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo
            = initializers::pipelineLayoutCreateInfo(setLayouts.data(), setLayouts.size());

        // Push constant to pass the material index for each mesh
        VkPushConstantRange pushConstantRange
            = initializers::pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(uint32_t), 0);
        pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
        pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
        CHECK_RESULT(vkCreatePipelineLayout(m_device,
            &pipelineLayoutCreateInfo,
            nullptr,
            &m_pipelineLayouts.raster))
    }

    // Ray Tracing
    m_rayTracing->createDescriptorSetsLayout(m_scene);

    // Postprocess
    m_postProcess->createDescriptorSetsLayout();

    // Auto exposure compute layout
    m_autoExposure->createDescriptorSetsLayout();
}

void HybridPipelineRT::createDescriptorSets()
{
    // Offscreen
    {
        // Set 0 Raster: Scene descriptor
        std::vector<VkDescriptorSetLayout> layouts(m_swapChain.imageCount,
            m_rasterDescriptorSetLayouts.set0Scene);
        VkDescriptorSetAllocateInfo allocInfo
            = initializers::descriptorSetAllocateInfo(m_descriptorPool,
                layouts.data(),
                m_swapChain.imageCount);
        m_rasterDescriptorSets.set0Scene.resize(m_swapChain.imageCount);
        CHECK_RESULT(
            vkAllocateDescriptorSets(m_device, &allocInfo, m_rasterDescriptorSets.set0Scene.data()))
        for (size_t i = 0; i < m_swapChain.imageCount; ++i) {
            std::vector<VkWriteDescriptorSet> writeDescriptorSet0 = {
                // Binding 0 : Vertex shader uniform buffer
                initializers::writeDescriptorSet(m_rasterDescriptorSets.set0Scene[i],
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    0,
                    &m_sceneBuffers[i].descriptor),
            };
            vkUpdateDescriptorSets(m_device,
                writeDescriptorSet0.size(),
                writeDescriptorSet0.data(),
                0,
                VK_NULL_HANDLE);
        }

        // Set 1 Raster: Material descriptor
        // Materials and Textures descriptor sets
        VkDescriptorSetAllocateInfo textureAllocInfo
            = initializers::descriptorSetAllocateInfo(m_descriptorPool,
                &m_rasterDescriptorSetLayouts.set1Materials,
                1);
        CHECK_RESULT(vkAllocateDescriptorSets(m_device,
            &textureAllocInfo,
            &m_rasterDescriptorSets.set1Materials))

        std::vector<VkWriteDescriptorSet> writeDescriptorSet1;
        VkWriteDescriptorSet writeTextureDescriptorSet;
        std::vector<VkDescriptorImageInfo> textureDescriptors;
        if (!m_scene->textures.empty()) {
            for (auto& texture : m_scene->textures) {
                textureDescriptors.push_back(texture.descriptor);
            }
            writeTextureDescriptorSet
                = initializers::writeDescriptorSet(m_rasterDescriptorSets.set1Materials,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    0,
                    textureDescriptors.data(),
                    textureDescriptors.size());
            writeDescriptorSet1.push_back(writeTextureDescriptorSet);
        }
        VkWriteDescriptorSet writeMaterialsDescriptorSet
            = initializers::writeDescriptorSet(m_rasterDescriptorSets.set1Materials,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                1,
                &m_materialsBuffer.descriptor);
        writeDescriptorSet1.push_back(writeMaterialsDescriptorSet);
        vkUpdateDescriptorSets(m_device,
            static_cast<uint32_t>(writeDescriptorSet1.size()),
            writeDescriptorSet1.data(),
            0,
            nullptr);

        // Set 2 Raster: Lighting descriptor
        VkDescriptorSetAllocateInfo set2AllocInfo
            = initializers::descriptorSetAllocateInfo(m_descriptorPool,
                &m_rasterDescriptorSetLayouts.set2Lights,
                1);
        CHECK_RESULT(
            vkAllocateDescriptorSets(m_device, &set2AllocInfo, &m_rasterDescriptorSets.set2Lights))
        VkWriteDescriptorSet writeLightsDescriptorSet
            = initializers::writeDescriptorSet(m_rasterDescriptorSets.set2Lights,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                0,
                &m_lightsBuffer.descriptor);

        std::vector<VkWriteDescriptorSet> writeDescriptorSet2 = { writeLightsDescriptorSet };
        vkUpdateDescriptorSets(m_device,
            static_cast<uint32_t>(writeDescriptorSet2.size()),
            writeDescriptorSet2.data(),
            0,
            nullptr);
    }

    // Ray Tracing
    m_rayTracing->createDescriptorSets(m_descriptorPool,
        m_scene,
        m_swapChain.imageCount,
        m_sceneBuffers,
        &m_instancesBuffer,
        &m_lightsBuffer,
        &m_materialsBuffer);

    // Postprocess
    m_postProcess->createDescriptorSets(m_descriptorPool,
        m_sceneBuffers,
        m_swapChain.imageCount,
        m_exposureBuffers,
        m_swapChain.imageCount);

    // Exposure compute
    m_autoExposure->createDescriptorSets(m_descriptorPool,
        m_exposureBuffers,
        m_swapChain.imageCount);

    updateResultImageDescriptorSets();
}

void HybridPipelineRT::createStorageImages()
{
    m_storageImages.resize(m_swapChain.imageCount);
    for (size_t i = 0; i < m_swapChain.imageCount; ++i) {
        m_storageImages[i].offscreenMaterial.toColorAttachment(VK_FORMAT_R32G32B32A32_SFLOAT,
            m_width,
            m_height,
            m_vulkanDevice,
            m_queue,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_SAMPLED_BIT);
        m_storageImages[i].offscreenAlbedo.toColorAttachment(VK_FORMAT_R8G8B8A8_UNORM,
            m_width,
            m_height,
            m_vulkanDevice,
            m_queue,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_SAMPLED_BIT);
        m_storageImages[i].offscreenNormals.toColorAttachment(VK_FORMAT_R32G32B32A32_SFLOAT,
            m_width,
            m_height,
            m_vulkanDevice,
            m_queue,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_SAMPLED_BIT);
        m_storageImages[i].offscreenReflectRefractMap.toColorAttachment(
            VK_FORMAT_R32G32B32A32_SFLOAT,
            m_width,
            m_height,
            m_vulkanDevice,
            m_queue,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_SAMPLED_BIT);
        m_storageImages[i].offscreenDepth.toDepthAttachment(VK_FORMAT_D32_SFLOAT,
            m_width,
            m_height,
            m_vulkanDevice,
            m_queue,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_SAMPLED_BIT);
        m_storageImages[i].rtResultImage.fromNothing(VK_FORMAT_R32G32B32A32_SFLOAT,
            m_width,
            m_height,
            1,
            m_vulkanDevice,
            m_queue,
            VK_FILTER_LINEAR,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        // Use R8G8B8A8_UNORM explicitly to match the shader format specification (rgba8)
        // The shader expects rgba8 which corresponds to VK_FORMAT_R8G8B8A8_UNORM
        m_storageImages[i].postProcessResultImage.fromNothing(VK_FORMAT_R8G8B8A8_UNORM,
            m_width,
            m_height,
            1,
            m_vulkanDevice,
            m_queue,
            VK_FILTER_NEAREST,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    }
}

void HybridPipelineRT::createOffscreenRenderPass()
{
    std::array<VkAttachmentDescription, 5> attachmentDescriptions = {};
    // Material attachment
    attachmentDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Albedo attachment
    attachmentDescriptions[1].format = VK_FORMAT_R8G8B8A8_UNORM;
    attachmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Normals attachment
    attachmentDescriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attachmentDescriptions[2].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Reflection and Refraction map attachment
    attachmentDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attachmentDescriptions[3].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[3].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Depth attachment
    attachmentDescriptions[4].format = VK_FORMAT_D32_SFLOAT;
    attachmentDescriptions[4].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[4].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[4].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[4].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[4].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference materialReference = {};
    materialReference.attachment = 0;
    materialReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference albedoReference = {};
    albedoReference.attachment = 1;
    albedoReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference normalsReference = {};
    normalsReference.attachment = 2;
    normalsReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference reflectRefractReference = {};
    reflectRefractReference.attachment = 3;
    reflectRefractReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    std::array<VkAttachmentReference, 4> colorAttachmentReferences
        = { materialReference, albedoReference, normalsReference, reflectRefractReference };

    VkAttachmentReference depthReference = {};
    depthReference.attachment = 4;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount
        = static_cast<uint32_t>(colorAttachmentReferences.size());
    subpassDescription.pColorAttachments = colorAttachmentReferences.data();
    subpassDescription.pDepthStencilAttachment = &depthReference;

    std::array<VkSubpassDependency, 2> dependencies = {};

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Create the actual renderpass
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
    renderPassInfo.pAttachments = attachmentDescriptions.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDescription;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();
    CHECK_RESULT(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_offscreenRenderPass))
}

void HybridPipelineRT::createOffscreenFramebuffers()
{
    m_offscreenFramebuffers.resize(m_swapChain.imageCount);
    for (uint32_t i = 0; i < m_swapChain.imageCount; ++i) {
        std::array<VkImageView, 5> attachments = {};
        attachments[0] = m_storageImages[i].offscreenMaterial.getImageView();
        attachments[1] = m_storageImages[i].offscreenAlbedo.getImageView();
        attachments[2] = m_storageImages[i].offscreenNormals.getImageView();
        attachments[3] = m_storageImages[i].offscreenReflectRefractMap.getImageView();
        attachments[4] = m_storageImages[i].offscreenDepth.getImageView();

        VkFramebufferCreateInfo framebufferCreateInfo {};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = m_offscreenRenderPass;
        framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferCreateInfo.pAttachments = attachments.data();
        framebufferCreateInfo.width = m_width;
        framebufferCreateInfo.height = m_height;
        framebufferCreateInfo.layers = 1;
        CHECK_RESULT(vkCreateFramebuffer(m_device,
            &framebufferCreateInfo,
            nullptr,
            &m_offscreenFramebuffers[i]))
    }
}

void HybridPipelineRT::updateResultImageDescriptorSets()
{
    for (uint32_t i = 0; i < m_swapChain.imageCount; ++i) {
        // Ray tracing sets
        m_rayTracing->updateResultImageDescriptorSets(i,
            &m_storageImages[i].offscreenMaterial,
            &m_storageImages[i].offscreenAlbedo,
            &m_storageImages[i].offscreenNormals,
            &m_storageImages[i].offscreenReflectRefractMap,
            &m_storageImages[i].offscreenDepth,
            &m_storageImages[i].rtResultImage);

        // Post Process
        m_postProcess->updateResultImageDescriptorSets(i,
            &m_storageImages[i].rtResultImage,
            &m_storageImages[i].postProcessResultImage);

        // Auto exposure
        m_autoExposure->updateResultImageDescriptorSets(i, &m_storageImages[i].rtResultImage);
    }
}

void HybridPipelineRT::updateUniformBuffers(uint32_t t_currentImage)
{
    memcpy(m_sceneBuffers[t_currentImage].mapped, &m_sceneUniformData, sizeof(m_sceneUniformData));
}

void HybridPipelineRT::onSwapChainRecreation()
{
    // Recreate the result image to fit the new extent size
    for (size_t i = 0; i < m_swapChain.imageCount; ++i) {
        m_storageImages[i].rtResultImage.destroy();
        m_storageImages[i].postProcessResultImage.destroy();
        m_storageImages[i].offscreenMaterial.destroy();
        m_storageImages[i].offscreenAlbedo.destroy();
        m_storageImages[i].offscreenDepth.destroy();
        m_storageImages[i].offscreenNormals.destroy();
        m_storageImages[i].offscreenReflectRefractMap.destroy();
        vkDestroyFramebuffer(m_device, m_offscreenFramebuffers[i], nullptr);
    }
    createStorageImages();
    createOffscreenFramebuffers();
    updateResultImageDescriptorSets();
}

// Prepare and initialize uniform buffer containing shader uniforms
void HybridPipelineRT::createUniformBuffers()
{
    VkDeviceSize bufferSize = sizeof(m_sceneUniformData);
    m_sceneBuffers.resize(m_swapChain.imageCount);
    for (size_t i = 0; i < m_swapChain.imageCount; ++i) {
        m_sceneBuffers[i].create(m_vulkanDevice,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            bufferSize);
        CHECK_RESULT(m_sceneBuffers[i].map())
    }
    // Instances Information uniform
    bufferSize = sizeof(ShaderMeshInstance) * m_scene->getInstancesCount();
    m_instancesBuffer.create(m_vulkanDevice,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        bufferSize,
        m_scene->getInstancesShaderData().data());
    // global Material list uniform (its a storage buffer)
    bufferSize = sizeof(ShaderMaterial) * m_scene->getMaterialCount();
    m_materialsBuffer.create(m_vulkanDevice,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        bufferSize,
        m_scene->getMaterialsShaderData().data());

    // global Light list uniform (its a storage buffer)
    bufferSize = sizeof(ShaderLight) * m_scene->getLightCount();
    m_lightsBuffer.create(m_vulkanDevice,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        bufferSize,
        m_scene->getLightsShaderData().data());

    // Auto Exposure uniform, also set the default data
    bufferSize = sizeof(ExposureUniformData);
    m_exposureBuffers.resize(m_swapChain.imageCount);
    for (size_t i = 0; i < m_swapChain.imageCount; ++i) {
        m_exposureBuffers[i].create(m_vulkanDevice,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            bufferSize);
        CHECK_RESULT(m_exposureBuffers[i].map())
        memcpy(m_exposureBuffers[i].mapped, &m_exposureData, bufferSize);
        m_exposureBuffers[i].unmap();
    }
}

void HybridPipelineRT::createRasterPipeline()
{
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState
        = initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            0,
            VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState
        = initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_BACK_BIT,
            VK_FRONT_FACE_CLOCKWISE,
            0);
    std::array<VkPipelineColorBlendAttachmentState, 4> blendAttachmentStates
        = { initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
              initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
              initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
              initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE) };
    VkPipelineColorBlendStateCreateInfo colorBlendState
        = initializers::pipelineColorBlendStateCreateInfo(blendAttachmentStates.size(),
            blendAttachmentStates.data());
    VkPipelineDepthStencilStateCreateInfo depthStencilState
        = initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE,
            VK_TRUE,
            VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState
        = initializers::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleState {};
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleState.flags = 0;
    std::vector<VkDynamicState> dynamicStateEnables
        = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState
        = initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(),
            dynamicStateEnables.size(),
            0);

    // Pipeline for the meshes
    // Load shaders
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    VkGraphicsPipelineCreateInfo pipelineCreateInfo
        = initializers::pipelineCreateInfo(m_pipelineLayouts.raster, m_offscreenRenderPass, 0);
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.stageCount = shaderStages.size();
    pipelineCreateInfo.pStages = shaderStages.data();
    pipelineCreateInfo.pVertexInputState = {};

    VkVertexInputBindingDescription vertexInputBinding
        = initializers::vertexInputBindingDescription(vertex_buffer_bind_id,
            m_scene->getVertexLayoutStride(),
            VK_VERTEX_INPUT_RATE_VERTEX);

    // Attribute descriptions
    // Describes memory layout and shader positions
    std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
        initializers::vertexInputAttributeDescription(vertex_buffer_bind_id,
            0,
            VK_FORMAT_R32G32B32_SFLOAT,
            0), // Location 0: Position
        initializers::vertexInputAttributeDescription(vertex_buffer_bind_id,
            1,
            VK_FORMAT_R32G32B32_SFLOAT,
            sizeof(float) * 3), // Location 1: Normal
        initializers::vertexInputAttributeDescription(vertex_buffer_bind_id,
            2,
            VK_FORMAT_R32G32B32_SFLOAT,
            sizeof(float) * 6), // Location 2: Tangent
        initializers::vertexInputAttributeDescription(vertex_buffer_bind_id,
            3,
            VK_FORMAT_R32G32_SFLOAT,
            sizeof(float) * 9), // Location 3: Texture coordinates
    };

    VkPipelineVertexInputStateCreateInfo vertexInputState
        = initializers::pipelineVertexInputStateCreateInfo();
    vertexInputState.vertexBindingDescriptionCount = 1;
    vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
    vertexInputState.vertexAttributeDescriptionCount
        = static_cast<uint32_t>(vertexInputAttributes.size());
    vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

    pipelineCreateInfo.pVertexInputState = &vertexInputState;

    // Default mesh rendering pipeline
    shaderStages[0] = loadShader("shaders/offscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = loadShader("shaders/offscreen.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    CHECK_RESULT(vkCreateGraphicsPipelines(m_device,
        m_pipelineCache,
        1,
        &pipelineCreateInfo,
        nullptr,
        &m_pipelines.raster))
}

void HybridPipelineRT::createPostprocessPipeline()
{
    m_postProcess->createPipeline(m_pipelineCache,
        loadShader("./shaders/post_process.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT));
}

void HybridPipelineRT::createAutoExposurePipeline()
{
    m_autoExposure->createPipeline(m_pipelineCache,
        loadShader("./shaders/auto_exposure.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT));
}

void HybridPipelineRT::createRTPipeline()
{
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages(6);
    shaderStages[SBT_RAY_GEN_INDEX]
        = loadShader("./shaders/raygen.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    shaderStages[SBT_MISS_INDEX]
        = loadShader("./shaders/miss.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR);
    shaderStages[SBT_SHADOW_MISS_INDEX]
        = loadShader("./shaders/shadow.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR);
    shaderStages[SBT_ANY_HIT_INDEX]
        = loadShader("./shaders/anyhit.rahit.spv", VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
    shaderStages[SBT_CLOSEST_HIT_INDEX]
        = loadShader("./shaders/closesthit.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    shaderStages[SBT_SHADOW_ANY_HIT_INDEX]
        = loadShader("./shaders/shadow.rahit.spv", VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
    /*
      Setup ray tracing shader groups
    */
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups(SBT_NUM_SHADER_GROUPS);
    for (auto& group : groups) {
        // Init all groups with some default values
        group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        group.generalShader = VK_SHADER_UNUSED_KHR;
        group.closestHitShader = VK_SHADER_UNUSED_KHR;
        group.anyHitShader = VK_SHADER_UNUSED_KHR;
        group.intersectionShader = VK_SHADER_UNUSED_KHR;
    }

    // Links shaders and types to ray tracing shader groups
    // Ray generation shader group
    groups[SBT_RAY_GEN_GROUP].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[SBT_RAY_GEN_GROUP].generalShader = SBT_RAY_GEN_INDEX;
    // Scene miss shader group
    groups[SBT_MISS_GROUP].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[SBT_MISS_GROUP].generalShader = SBT_MISS_INDEX;
    // Shadow miss shader group
    groups[SBT_SHADOW_MISS_GROUP].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[SBT_SHADOW_MISS_GROUP].generalShader = SBT_SHADOW_MISS_INDEX;
    // Any hit shader group and closest hit shader group
    groups[SBT_HIT_GROUP].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groups[SBT_HIT_GROUP].anyHitShader = SBT_ANY_HIT_INDEX;
    groups[SBT_HIT_GROUP].closestHitShader = SBT_CLOSEST_HIT_INDEX;
    // Shadow closest hit shader group
    groups[SBT_SHADOW_HIT_GROUP].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groups[SBT_SHADOW_HIT_GROUP].anyHitShader = SBT_SHADOW_ANY_HIT_INDEX;

    m_rayTracing->createPipeline(shaderStages, groups);
}

void HybridPipelineRT::setupScene()
{
    SceneVertexLayout m_vertexLayout = SceneVertexLayout({ VERTEX_COMPONENT_POSITION,
        VERTEX_COMPONENT_NORMAL,
        VERTEX_COMPONENT_TANGENT,
        VERTEX_COMPONENT_UV,
        VERTEX_COMPONENT_DUMMY_FLOAT });
    m_scene = m_rayTracing->createRTScene(m_queue, "assets/pool/Pool_I.fbx", m_vertexLayout);
    auto camera = m_scene->getCamera();
    camera->setMovementSpeed(100.0f);
    camera->setRotationSpeed(0.5f);
    camera->setPerspective(60.0f,
        static_cast<float>(m_width) / static_cast<float>(m_height),
        CAMERA_NEAR,
        CAMERA_FAR);
}

void HybridPipelineRT::prepare()
{
    BaseProject::prepare();

    m_rayTracing = new HyRayTracingPipeline(m_vulkanDevice, 8, 1);
    m_autoExposure = new AutoExposurePipeline(m_vulkanDevice);
    m_postProcess = new PostProcessPipeline(m_vulkanDevice);

    setupScene();

    createStorageImages();
    createOffscreenRenderPass();
    createOffscreenFramebuffers();
    createUniformBuffers();
    createDescriptorSetLayout();
    createRasterPipeline();
    createRTPipeline();
    createPostprocessPipeline();
    createAutoExposurePipeline();
    createDescriptorPool();
    createDescriptorSets();
    buildCommandBuffers();
    m_prepared = true;
}

HybridPipelineRT::~HybridPipelineRT()
{
    delete m_rayTracing;
    delete m_autoExposure;
    delete m_postProcess;

    vkDestroyRenderPass(m_device, m_offscreenRenderPass, nullptr);
    for (auto& frameBuffer : m_offscreenFramebuffers) {
        vkDestroyFramebuffer(m_device, frameBuffer, nullptr);
    }

    vkDestroyPipeline(m_device, m_pipelines.raster, nullptr);

    vkDestroyPipelineLayout(m_device, m_pipelineLayouts.raster, nullptr);

    vkDestroyDescriptorSetLayout(m_device, m_rasterDescriptorSetLayouts.set0Scene, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rasterDescriptorSetLayouts.set1Materials, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rasterDescriptorSetLayouts.set2Lights, nullptr);

    for (auto& offscreenImage : m_storageImages) {
        offscreenImage.rtResultImage.destroy();
        offscreenImage.postProcessResultImage.destroy();
        offscreenImage.offscreenMaterial.destroy();
        offscreenImage.offscreenAlbedo.destroy();
        offscreenImage.offscreenDepth.destroy();
        offscreenImage.offscreenNormals.destroy();
        offscreenImage.offscreenReflectRefractMap.destroy();
    }

    for (size_t i = 0; i < m_swapChain.imageCount; ++i) {
        m_sceneBuffers[i].destroy();
        m_exposureBuffers[i].destroy();
    }
    m_materialsBuffer.destroy();
    m_instancesBuffer.destroy();
    m_lightsBuffer.destroy();
    m_scene->destroy();
}

void HybridPipelineRT::onKeyEvent(int t_key, int t_scancode, int t_action, int t_mods)
{
    switch (t_key) {
    case GLFW_KEY_J:
        m_sceneUniformData.overrideSunDirection.x += 0.05;
        viewChanged();
        break;
    case GLFW_KEY_K:
        m_sceneUniformData.overrideSunDirection.x -= 0.05;
        viewChanged();
        break;
    case GLFW_KEY_G:
        m_sceneUniformData.manualExposureAdjust += 0.1;
        break;
    case GLFW_KEY_H:
        m_sceneUniformData.manualExposureAdjust -= 0.1;
        break;
    default:
        break;
    }
}

void HybridPipelineRT::viewChanged()
{
    auto camera = m_scene->getCamera();
    camera->setPerspective(60.0f,
        static_cast<float>(m_width) / static_cast<float>(m_height),
        CAMERA_NEAR,
        CAMERA_FAR);
    m_sceneUniformData.projection = camera->matrices.perspective;
    m_sceneUniformData.view = camera->matrices.view;
    m_sceneUniformData.model = glm::mat4(1.0f);
    m_sceneUniformData.viewInverse = glm::inverse(camera->matrices.view);
    m_sceneUniformData.projInverse = glm::inverse(camera->matrices.perspective);
}
