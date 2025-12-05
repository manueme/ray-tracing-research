/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "ray_tracing_optix_denoiser.h"
#include "auto_exposure_pipeline.h"
#include "constants.h"
#include "cuda_optix_interop/denoiser_optix_pipeline.h"
#include "pipelines/denoise_ray_tracing_pipeline.h"
#include "post_process_pipeline.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

RayTracingOptixDenoiser::RayTracingOptixDenoiser()
    : BaseProject("Monte Carlo Ray Tracing With Optix Denoiser",
          "Monte Carlo Ray Tracing With Optix Denoiser", true)
{
    m_settings.vsync = false;
    m_settings.useCompute = true;
    m_settings.useRayTracing = true;
    // Make sure no more than 1 frame is processed at the same time to
    // avoid issues in the accumulated image

    m_enabledInstanceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
    m_enabledInstanceExtensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME);
    m_enabledInstanceExtensions.push_back(VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME);
}

void RayTracingOptixDenoiser::buildCommandBuffers()
{
    // Draw command buffers
    VkCommandBufferBeginInfo drawCmdBufInfo = {};
    drawCmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    // Postprocess pass info
    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = m_default_clear_color;
    clearValues[1].depthStencil = { 1.0f, 0 };
    VkRenderPassBeginInfo renderPassBeginInfo = {};

    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = m_renderPass;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = m_width;
    renderPassBeginInfo.renderArea.extent.height = m_height;
    renderPassBeginInfo.clearValueCount = clearValues.size();
    renderPassBeginInfo.pClearValues = clearValues.data();
    // --

    for (int32_t i = 0; i < m_drawCmdBuffers.size(); ++i) {
        CHECK_RESULT(vkBeginCommandBuffer(m_drawCmdBuffers[i], &drawCmdBufInfo))
        m_rayTracing->buildCommandBuffer(m_drawCmdBuffers[i], m_width, m_height);
        CHECK_RESULT(vkEndCommandBuffer(m_drawCmdBuffers[i]))
    }
    // ---
    // Compute command buffers
    VkCommandBufferBeginInfo computeCmdBufInfo = {};
    computeCmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    for (int32_t i = 0; i < m_compute.commandBuffers.size(); ++i) {
        CHECK_RESULT(vkBeginCommandBuffer(m_compute.commandBuffers[i], &computeCmdBufInfo))
        m_autoExposure->buildCommandBuffer(m_compute.commandBuffers[i]);
        m_postProcess->buildCommandBuffer(m_compute.commandBuffers[i], m_width, m_height);

        // Move post process output to swap chain image:

        // Prepare images to transfer
        VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        tools::setImageLayout(m_compute.commandBuffers[i],
            m_swapChain.images[i],
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);
        tools::setImageLayout(m_compute.commandBuffers[i],
            m_storageImage.postProcessResult.getImage(),
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
            m_storageImage.postProcessResult.getImage(),
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
            m_storageImage.postProcessResult.getImage(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL,
            subresourceRange);

        vkEndCommandBuffer(m_compute.commandBuffers[i]);
    }
    // ---
}

void RayTracingOptixDenoiser::createDescriptorPool()
{
    // Calculate the number of textures needed
    uint32_t textureCount
        = m_scene->textures.empty() ? 1 : static_cast<uint32_t>(m_scene->textures.size());

    // Storage images: ray tracing set5ResultImages (1 binding) + postprocess set3ResultImage (1
    // binding) = 2 total
    uint32_t storageImageCount = 2;

    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
        // Scene description
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        // Exposure
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
        // Vertex, Index and Material Indexes
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
        // Textures (needs to accommodate all textures in the scene)
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, textureCount },
        // Material array
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
        // Lights array
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
        // Result images
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        // Storage images (ray tracing result image + postprocess result image)
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, storageImageCount },
    };
    // Calculate max set for pool
    uint32_t maxSetsForPool = 20;
    // ---

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo
        = initializers::descriptorPoolCreateInfo(poolSizes.size(),
            poolSizes.data(),
            maxSetsForPool);
    CHECK_RESULT(
        vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool));
}

void RayTracingOptixDenoiser::createDescriptorSetsLayout()
{
    // Ray Tracing
    m_rayTracing->createDescriptorSetsLayout(m_scene);

    // Postprocess
    m_postProcess->createDescriptorSetsLayout();

    // Auto exposure compute layout
    m_autoExposure->createDescriptorSetsLayout();
}

void RayTracingOptixDenoiser::createPostprocessPipeline()
{
    m_postProcess->createPipeline(m_pipelineCache,
        loadShader("./shaders/post_process.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT));
}

void RayTracingOptixDenoiser::createRTPipeline()
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

void RayTracingOptixDenoiser::createAutoExposurePipeline()
{
    m_autoExposure->createPipeline(m_pipelineCache,
        loadShader("./shaders/auto_exposure.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT));
}

void RayTracingOptixDenoiser::createDescriptorSets()
{
    // Ray Tracing
    m_rayTracing->createDescriptorSets(m_descriptorPool,
        m_scene,
        &m_sceneBuffer,
        &m_instancesBuffer,
        &m_lightsBuffer,
        &m_materialsBuffer);

    // Postprocess
    m_postProcess->createDescriptorSets(m_descriptorPool, &m_sceneBuffer, &m_exposureBuffer);

    // Exposure compute
    m_autoExposure->createDescriptorSets(m_descriptorPool, &m_exposureBuffer);

    updateResultImageDescriptorSets();
}

void RayTracingOptixDenoiser::updateResultImageDescriptorSets()
{
    // Ray Tracing
    m_rayTracing->updateResultImageDescriptorSets(&m_storageImage.depthMap,
        &m_denoiserData.pixelBufferInAlbedo,
        &m_denoiserData.pixelBufferInNormal,
        &m_denoiserData.pixelBufferInPixelFlow,
        &m_denoiserData.pixelBufferInRawResult);

    // Post Process
    m_postProcess->updateResultImageDescriptorSets(&m_denoiserData.pixelBufferOut,
        &m_storageImage.postProcessResult);

    // Exposure compute
    m_autoExposure->updateResultImageDescriptorSets(&m_denoiserData.pixelBufferOut);
}

void RayTracingOptixDenoiser::updateUniformBuffers(uint32_t t_currentImage)
{
    // "max frames in flight" is 1 so this shouldn't run concurrently, ignore t_currentImage
    memcpy(m_sceneBuffer.mapped, &m_sceneUniformData, sizeof(UniformData));
}

// Prepare and initialize uniform buffer containing shader uniforms
void RayTracingOptixDenoiser::createUniformBuffers()
{
    // Scene uniform
    VkDeviceSize bufferSize = sizeof(UniformData);
    m_sceneBuffer.create(m_vulkanDevice,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        bufferSize);
    CHECK_RESULT(m_sceneBuffer.map())

    // Instances Information uniform
    bufferSize = sizeof(ShaderMeshInstance) * m_scene->getInstancesCount();
    m_instancesBuffer.create(m_vulkanDevice,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        bufferSize,
        m_scene->getInstancesShaderData().data());

    // Global Material list uniform (its a storage buffer)
    bufferSize = sizeof(ShaderMaterial) * m_scene->getMaterialCount();
    m_materialsBuffer.create(m_vulkanDevice,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        bufferSize,
        m_scene->getMaterialsShaderData().data());

    // Global Light list uniform (its a storage buffer)
    bufferSize = sizeof(ShaderLight) * m_scene->getLightCount();
    m_lightsBuffer.create(m_vulkanDevice,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        bufferSize,
        m_scene->getLightsShaderData().data());

    // Auto Exposure buffer, also set the default data
    bufferSize = sizeof(ExposureUniformData);
    m_exposureBuffer.create(m_vulkanDevice,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        bufferSize);
    CHECK_RESULT(m_exposureBuffer.map())
    memcpy(m_exposureBuffer.mapped, &m_exposureData, bufferSize);
    m_exposureBuffer.unmap();
}

void RayTracingOptixDenoiser::createStorageImages()
{
    m_storageImage.depthMap.fromNothing(VK_FORMAT_R32_SFLOAT,
        m_width,
        m_height,
        1,
        m_vulkanDevice,
        m_queue,
        VK_FILTER_LINEAR,
        VK_IMAGE_USAGE_STORAGE_BIT,
        VK_IMAGE_LAYOUT_GENERAL);

    // Use R8G8B8A8_UNORM explicitly to match the shader format specification (rgba8)
    // The shader expects rgba8 which corresponds to VK_FORMAT_R8G8B8A8_UNORM
    m_storageImage.postProcessResult.fromNothing(VK_FORMAT_R8G8B8A8_UNORM,
        m_width,
        m_height,
        1,
        m_vulkanDevice,
        m_queue,
        VK_FILTER_NEAREST,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_IMAGE_LAYOUT_GENERAL);
}

void RayTracingOptixDenoiser::setupScene()
{
    SceneVertexLayout m_vertexLayout = SceneVertexLayout({ VERTEX_COMPONENT_POSITION,
        VERTEX_COMPONENT_NORMAL,
        VERTEX_COMPONENT_TANGENT,
        VERTEX_COMPONENT_UV,
        VERTEX_COMPONENT_DUMMY_FLOAT });
    m_scene
        = m_rayTracing->createRTScene(m_queue, "assets/cornellbox/Cornellbox.fbx", m_vertexLayout);
    auto camera = m_scene->getCamera();
    camera->setMovementSpeed(100.0f);
    camera->setRotationSpeed(0.5f);
    camera->setPerspective(60.0f,
        static_cast<float>(m_width) / static_cast<float>(m_height),
        CAMERA_NEAR,
        CAMERA_FAR);
}

void RayTracingOptixDenoiser::prepare()
{
    BaseProject::prepare();

    m_rayTracing = new DenoiseRayTracingPipeline(m_vulkanDevice, 10, 1);
    m_autoExposure = new AutoExposureWithBuffersPipeline(m_vulkanDevice);
    m_postProcess = new PostProcessWithBuffersPipeline(m_vulkanDevice);

    m_denoiser = new DenoiserOptixPipeline(m_vulkanDevice);
    m_denoiserData.denoiseWaitFor.create(m_device);
    m_denoiserData.denoiseSignalTo.create(m_device);

    m_denoiser->allocateBuffers({ m_width, m_height },
        &m_denoiserData.pixelBufferInRawResult,
        &m_denoiserData.pixelBufferInAlbedo,
        &m_denoiserData.pixelBufferInNormal,
        &m_denoiserData.pixelBufferInPixelFlow,
        &m_denoiserData.pixelBufferOut);

    setupScene();

    createStorageImages();
    createUniformBuffers();
    createDescriptorSetsLayout();
    createPostprocessPipeline();
    createRTPipeline();
    createAutoExposurePipeline();
    createDescriptorPool();
    createDescriptorSets();
    buildCommandBuffers();
    m_prepared = true;
}

void RayTracingOptixDenoiser::render()
{
    if (!m_prepared) {
        return;
    }
    const auto imageIndex = BaseProject::acquireNextImage();

    // Skip frame if acquisition failed
    if (imageIndex == UINT32_MAX) {
        return;
    }

    // Get the frame index that was used for acquisition (needed for the acquisition semaphore)
    size_t frameIndex = getAcquisitionFrameIndex(imageIndex);

    vkWaitForFences(m_device, 1, &m_compute.fences[imageIndex], VK_TRUE, UINT64_MAX);

    updateUniformBuffers(imageIndex);

    auto denoiserWaitForSemaphore = m_denoiserData.denoiseWaitFor.getVulkanSemaphore();
    auto denoiserSignalToSemaphore = m_denoiserData.denoiseSignalTo.getVulkanSemaphore();

    // Submit the draw command buffer
    ++m_denoiserData.timelineValue;
    VkTimelineSemaphoreSubmitInfo timelineInfo {};
    timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timelineInfo.signalSemaphoreValueCount = 1;
    timelineInfo.pSignalSemaphoreValues = &m_denoiserData.timelineValue;
    timelineInfo.waitSemaphoreValueCount = 1;
    timelineInfo.pWaitSemaphoreValues = &m_denoiserData.timelineValue;

    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = &timelineInfo;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[frameIndex];
    VkPipelineStageFlags drawWaitStageMask = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    submitInfo.pWaitDstStageMask = &drawWaitStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_drawCmdBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &denoiserWaitForSemaphore;
    vkResetFences(m_device, 1, &m_inFlightFences[imageIndex]);
    CHECK_RESULT(vkQueueSubmit(m_queue, 1, &submitInfo, m_inFlightFences[imageIndex]))
    // ----

    m_denoiser->denoiseSubmit(&m_denoiserData.denoiseWaitFor,
        &m_denoiserData.denoiseSignalTo,
        0.0f,
        m_sceneUniformData.frame == 0,
        m_denoiserData.timelineValue);

    // Submit Compute Command Buffer:
    VkSubmitInfo computeSubmitInfo {};
    computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    computeSubmitInfo.commandBufferCount = 1;
    computeSubmitInfo.pCommandBuffers = &m_compute.commandBuffers[imageIndex];
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &denoiserSignalToSemaphore;
    drawWaitStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    submitInfo.pWaitDstStageMask = &drawWaitStageMask;
    computeSubmitInfo.signalSemaphoreCount = 1;
    computeSubmitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[imageIndex];
    VkPipelineStageFlags computeWaitStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    computeSubmitInfo.pWaitDstStageMask = &computeWaitStageMask;
    vkResetFences(m_device, 1, &m_compute.fences[imageIndex]);
    CHECK_RESULT(
        vkQueueSubmit(m_compute.queue, 1, &computeSubmitInfo, m_compute.fences[imageIndex]));
    // ----

    if (BaseProject::queuePresentSwapChain(imageIndex) == VK_SUCCESS) {
        std::cout << "| FPS: " << m_lastFps << " -- Sample: " << m_sceneUniformData.frameIteration
                  << " | " << std::endl;

        m_sceneUniformData.frameChanged = 0;
        ++m_sceneUniformData.frame;
        ++m_sceneUniformData.frameIteration;
        m_sceneUniformData.prevProjection = m_sceneUniformData.currentProjection;
        m_sceneUniformData.prevView = m_sceneUniformData.currentView;

        // MSE calculation screenshots
        //        if (m_sceneUniformData.frame == 1 ||
        //            m_sceneUniformData.frame == 10 ||
        //            m_sceneUniformData.frame == 50 ||
        //            m_sceneUniformData.frame == 100 ||
        //            m_sceneUniformData.frame == 500 ||
        //            m_sceneUniformData.frame == 1000 ||
        //            m_sceneUniformData.frame == 5000 ||
        //            m_sceneUniformData.frame == 10000 ||
        //            m_sceneUniformData.frame == 50000) {
        //            saveScreenshot(std::string("./monte_carlo_denoiser_exterior_" +
        //            std::to_string(m_sceneUniformData.frame) + ".ppm").c_str());
        //        }
    }
}

RayTracingOptixDenoiser::~RayTracingOptixDenoiser()
{
    delete m_rayTracing;
    delete m_autoExposure;
    delete m_postProcess;
    delete m_denoiser;
    m_denoiserData.denoiseWaitFor.destroy();
    m_denoiserData.denoiseSignalTo.destroy();

    m_storageImage.postProcessResult.destroy();
    m_storageImage.depthMap.destroy();

    m_denoiserData.pixelBufferInAlbedo.destroy();
    m_denoiserData.pixelBufferInNormal.destroy();
    m_denoiserData.pixelBufferInPixelFlow.destroy();
    m_denoiserData.pixelBufferInRawResult.destroy();
    m_denoiserData.pixelBufferOut.destroy();

    m_exposureBuffer.destroy();
    m_sceneBuffer.destroy();
    m_materialsBuffer.destroy();
    m_instancesBuffer.destroy();
    m_lightsBuffer.destroy();
    m_scene->destroy();
}

void RayTracingOptixDenoiser::viewChanged()
{
    auto camera = m_scene->getCamera();
    camera->setPerspective(60.0f,
        static_cast<float>(m_width) / static_cast<float>(m_height),
        CAMERA_NEAR,
        CAMERA_FAR);
    if (m_sceneUniformData.frame == 0) {
        m_sceneUniformData.prevProjection = camera->matrices.perspective;
        m_sceneUniformData.prevView = camera->matrices.view;
    } else {
        m_sceneUniformData.prevProjection = m_sceneUniformData.currentProjection;
        m_sceneUniformData.prevView = m_sceneUniformData.currentView;
    }
    m_sceneUniformData.currentProjection = camera->matrices.perspective;
    m_sceneUniformData.currentView = camera->matrices.view;

    m_sceneUniformData.projInverse = glm::inverse(camera->matrices.perspective);
    m_sceneUniformData.viewInverse = glm::inverse(camera->matrices.view);
    m_sceneUniformData.frameIteration = 0;
}

void RayTracingOptixDenoiser::onSwapChainRecreation()
{
    // Recreate the images to fit the new extent size
    m_storageImage.postProcessResult.destroy();
    m_storageImage.depthMap.destroy();
    createStorageImages();
    m_denoiserData.pixelBufferInAlbedo.destroy();
    m_denoiserData.pixelBufferInNormal.destroy();
    m_denoiserData.pixelBufferInPixelFlow.destroy();
    m_denoiserData.pixelBufferInRawResult.destroy();
    m_denoiserData.pixelBufferOut.destroy();
    m_denoiser->allocateBuffers({ m_width, m_height },
        &m_denoiserData.pixelBufferInRawResult,
        &m_denoiserData.pixelBufferInAlbedo,
        &m_denoiserData.pixelBufferInNormal,
        &m_denoiserData.pixelBufferInPixelFlow,
        &m_denoiserData.pixelBufferOut);

    updateResultImageDescriptorSets();
}

void RayTracingOptixDenoiser::onKeyEvent(int t_key, int t_scancode, int t_action, int t_mods)
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
void RayTracingOptixDenoiser::windowResized()
{
    BaseProject::windowResized();
    m_sceneUniformData.frameChanged = 1;
    m_sceneUniformData.frame = 0;
}

void RayTracingOptixDenoiser::getEnabledFeatures()
{
    BaseProject::getEnabledFeatures();
    m_enabledDeviceExtensions.emplace_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
    m_enabledDeviceExtensions.emplace_back(VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
    m_enabledDeviceExtensions.emplace_back(VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME);
#ifdef WIN32
    m_enabledDeviceExtensions.emplace_back(VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME);
    m_enabledDeviceExtensions.emplace_back(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
    m_enabledDeviceExtensions.emplace_back(VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME);
#else
    m_enabledDeviceExtensions.emplace_back(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
    m_enabledDeviceExtensions.emplace_back(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    m_enabledDeviceExtensions.emplace_back(VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME);
#endif

    timelineFeature = {};
    timelineFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    timelineFeature.timelineSemaphore = VK_TRUE;
    timelineFeature.pNext = m_deviceCreatedNextChain;
    m_deviceCreatedNextChain = &timelineFeature;
    m_enabledDeviceExtensions.emplace_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
}
