/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "monte_carlo_ray_tracing.h"
#include "common/auto_exposure_pipeline.h"
#include "common/ray_tracing_pipeline.h"
#include "constants.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

MonteCarloRTApp::MonteCarloRTApp()
    : BaseRTProject("Monte Carlo Ray Tracing", "Monte Carlo Ray Tracing App", true)
{
    m_settings.vsync = false;
    m_settings.useCompute = true;
    // Make sure no more than 1 frame is processed at the same time to
    // avoid issues in the accumulated image
    m_maxFramesInFlight = 1;
}

void MonteCarloRTApp::getEnabledFeatures() { BaseRTProject::getEnabledFeatures(); }

void MonteCarloRTApp::buildCommandBuffers()
{
    // Draw command buffers
    {
        VkCommandBufferBeginInfo cmdBufInfo = {};
        cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

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

        for (int32_t i = 0; i < m_drawCmdBuffers.size();
             ++i) { // This must be the same size as the swap chain image vector
            CHECK_RESULT(vkBeginCommandBuffer(m_drawCmdBuffers[i], &cmdBufInfo))

            m_rayTracing->buildCommandBuffer(m_drawCmdBuffers[i], m_width, m_height);

            // Postprocess section:
            renderPassBeginInfo.framebuffer = m_frameBuffers[i];
            vkCmdBeginRenderPass(m_drawCmdBuffers[i],
                &renderPassBeginInfo,
                VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport = initializers::viewport(static_cast<float>(m_width),
                static_cast<float>(m_height),
                0.0f,
                1.0f);
            vkCmdSetViewport(m_drawCmdBuffers[i], 0, 1, &viewport);
            VkRect2D scissor = initializers::rect2D(m_width, m_height, 0, 0);
            vkCmdSetScissor(m_drawCmdBuffers[i], 0, 1, &scissor);
            std::vector<VkDescriptorSet> postprocessDescriptorSets
                = { m_postprocessDescriptorSets.set0Scene,
                      m_postprocessDescriptorSets.set1InputImage,
                      m_postprocessDescriptorSets.set2Exposure };
            vkCmdBindDescriptorSets(m_drawCmdBuffers[i],
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_pipelineLayouts.postProcess,
                0,
                postprocessDescriptorSets.size(),
                postprocessDescriptorSets.data(),
                0,
                nullptr);
            vkCmdBindPipeline(m_drawCmdBuffers[i],
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_pipelines.postProcess);
            vkCmdDraw(m_drawCmdBuffers[i], 3, 1, 0, 0);

            vkCmdEndRenderPass(m_drawCmdBuffers[i]);
            // End of Postprocess section --

            CHECK_RESULT(vkEndCommandBuffer(m_drawCmdBuffers[i]))
        }
    }
    // ---
    // Compute command buffers
    VkCommandBufferBeginInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CHECK_RESULT(vkBeginCommandBuffer(m_compute.commandBuffer, &cmdBufInfo))
    m_autoExposure->buildCommandBuffer(m_compute.commandBuffer);
    vkEndCommandBuffer(m_compute.commandBuffer);
    // ---
}

void MonteCarloRTApp::createDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
        // Scene description
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        // Exposure
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
        // Vertex, Index and Material Indexes
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
        // Textures
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        // Material array
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
        // Lights array
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
        // Result images
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
    };
    // Calculate max set for pool
    const auto asSets = 1;
    const auto sceneSets = 1;
    const auto vertexAndIndexes = 3;
    const auto textures = 1;
    const auto materials = 1;
    const auto lights = 1;
    const auto resultImages = 4;
    uint32_t maxSetsForPool
        = asSets + sceneSets + vertexAndIndexes + textures + materials + lights + resultImages;
    // ---

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo
        = initializers::descriptorPoolCreateInfo(poolSizes.size(),
            poolSizes.data(),
            maxSetsForPool);
    CHECK_RESULT(
        vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool));
}

void MonteCarloRTApp::createDescriptorSetsLayout()
{
    // Ray Tracing
    m_rayTracing->createDescriptorSetsLayout(m_scene);

    // Postprocess
    {
        // Set 0 Postprocess: Scene information buffer
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = { // Binding 0 : Buffer
            initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                0)
        };
        VkDescriptorSetLayoutCreateInfo descriptorLayout
            = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
                setLayoutBindings.size());
        CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
            &descriptorLayout,
            nullptr,
            &m_postprocessDescriptorSetLayouts.set0Scene));

        // Set 1 Postprocess: Input Image
        setLayoutBindings.clear();
        setLayoutBindings.push_back(
            // Binding 0 : Result Image Color
            initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                0));
        descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
            setLayoutBindings.size());
        CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
            &descriptorLayout,
            nullptr,
            &m_postprocessDescriptorSetLayouts.set1InputImage));

        // Set 2 Postprocess: Exposure buffer
        setLayoutBindings.clear();
        setLayoutBindings.push_back( // Binding 0 : Buffer
            initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                0));
        descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
            setLayoutBindings.size());
        CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
            &descriptorLayout,
            nullptr,
            &m_postprocessDescriptorSetLayouts.set2Exposure));

        // Postprocess Pipeline Layout
        std::array<VkDescriptorSetLayout, 3> postprocessSetLayouts
            = { m_postprocessDescriptorSetLayouts.set0Scene,
                  m_postprocessDescriptorSetLayouts.set1InputImage,
                  m_postprocessDescriptorSetLayouts.set2Exposure };
        VkPipelineLayoutCreateInfo postprocessPipelineLayoutCreateInfo
            = initializers::pipelineLayoutCreateInfo(postprocessSetLayouts.data(),
                postprocessSetLayouts.size());
        CHECK_RESULT(vkCreatePipelineLayout(m_device,
            &postprocessPipelineLayoutCreateInfo,
            nullptr,
            &m_pipelineLayouts.postProcess))
    }

    // Auto exposure compute layout
    m_autoExposure->createDescriptorSetsLayout();
}

void MonteCarloRTApp::createPostprocessPipeline()
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
    VkPipelineColorBlendAttachmentState blendAttachmentState
        = initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendState
        = initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
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
    VkPipelineVertexInputStateCreateInfo vertexInputState
        = initializers::pipelineVertexInputStateCreateInfo();
    vertexInputState.vertexBindingDescriptionCount = 0;
    vertexInputState.pVertexBindingDescriptions = nullptr;
    vertexInputState.vertexAttributeDescriptionCount = 0;
    vertexInputState.pVertexAttributeDescriptions = nullptr;

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    shaderStages[0] = loadShader("./shaders/postprocess.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = loadShader("./shaders/postprocess.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VkGraphicsPipelineCreateInfo pipelineCreateInfo
        = initializers::pipelineCreateInfo(m_pipelineLayouts.postProcess, m_renderPass, 0);
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.stageCount = shaderStages.size();
    pipelineCreateInfo.pStages = shaderStages.data();
    pipelineCreateInfo.pVertexInputState = &vertexInputState;
    CHECK_RESULT(vkCreateGraphicsPipelines(m_device,
        m_pipelineCache,
        1,
        &pipelineCreateInfo,
        nullptr,
        &m_pipelines.postProcess))
}

void MonteCarloRTApp::createRTPipeline()
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

void MonteCarloRTApp::createAutoExposurePipeline()
{
    m_autoExposure->createPipeline(m_pipelineCache,
        loadShader("./shaders/auto_exposure.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT));
}

void MonteCarloRTApp::createDescriptorSets()
{
    // Ray Tracing
    m_rayTracing->createDescriptorSets(m_descriptorPool,
        m_topLevelAS.getHandle(),
        m_scene,
        &m_sceneBuffer,
        &m_instancesBuffer,
        &m_lightsBuffer,
        &m_materialsBuffer);

    // Postprocess
    {
        // Postprocess input scene descriptor set, set 0
        VkDescriptorSetAllocateInfo allocInfo
            = initializers::descriptorSetAllocateInfo(m_descriptorPool,
                &m_postprocessDescriptorSetLayouts.set0Scene,
                1);
        CHECK_RESULT(
            vkAllocateDescriptorSets(m_device, &allocInfo, &m_postprocessDescriptorSets.set0Scene));
        std::vector<VkWriteDescriptorSet> writeDescriptorSet0 = {
            // Binding 0 : Uniform buffer
            initializers::writeDescriptorSet(m_postprocessDescriptorSets.set0Scene,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                0,
                &m_sceneBuffer.descriptor),
        };
        vkUpdateDescriptorSets(m_device,
            writeDescriptorSet0.size(),
            writeDescriptorSet0.data(),
            0,
            VK_NULL_HANDLE);

        // Postprocess input image descriptor set, set 1
        VkDescriptorSetAllocateInfo inputImageAllocateInfo
            = initializers::descriptorSetAllocateInfo(m_descriptorPool,
                &m_postprocessDescriptorSetLayouts.set1InputImage,
                1);
        CHECK_RESULT(vkAllocateDescriptorSets(m_device,
            &inputImageAllocateInfo,
            &m_postprocessDescriptorSets.set1InputImage));

        // Postprocess input manualExposureAdjust descriptor set, set 2
        allocInfo = initializers::descriptorSetAllocateInfo(m_descriptorPool,
            &m_postprocessDescriptorSetLayouts.set2Exposure,
            1);
        CHECK_RESULT(vkAllocateDescriptorSets(m_device,
            &allocInfo,
            &m_postprocessDescriptorSets.set2Exposure));
        std::vector<VkWriteDescriptorSet> writeDescriptorSet2 = {
            // Binding 2 : Uniform buffer
            initializers::writeDescriptorSet(m_postprocessDescriptorSets.set2Exposure,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                0,
                &m_exposureBuffer.descriptor),
        };
        vkUpdateDescriptorSets(m_device,
            writeDescriptorSet2.size(),
            writeDescriptorSet2.data(),
            0,
            VK_NULL_HANDLE);
    }

    // Exposure compute
    m_autoExposure->createDescriptorSets(m_descriptorPool, &m_exposureBuffer);

    updateResultImageDescriptorSets();
}

void MonteCarloRTApp::updateResultImageDescriptorSets()
{
    // Ray Tracing
    m_rayTracing->updateResultImageDescriptorSets(&m_storageImage.result,
        &m_storageImage.depthMap,
        &m_storageImage.normalMap,
        &m_storageImage.albedo);

    // Post Process
    {
        VkWriteDescriptorSet inputImageWrite
            = initializers::writeDescriptorSet(m_postprocessDescriptorSets.set1InputImage,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                0,
                &m_storageImage.result.descriptor);
        std::vector<VkWriteDescriptorSet> writeDescriptorSet1Postprocess = { inputImageWrite };
        vkUpdateDescriptorSets(m_device,
            static_cast<uint32_t>(writeDescriptorSet1Postprocess.size()),
            writeDescriptorSet1Postprocess.data(),
            0,
            VK_NULL_HANDLE);
    }

    // Auto exposure
    m_autoExposure->updateResultImageDescriptorSets(&m_storageImage.result);
}

void MonteCarloRTApp::updateUniformBuffers(uint32_t t_currentImage)
{
    // As long as we use accumulation in one single image it doesnt make sense to have multiple
    // scene buffers to update, m_maxFramesInFlight has to be 1 for this application
    memcpy(m_sceneBuffer.mapped, &m_sceneUniformData, sizeof(UniformData));
}

// Prepare and initialize uniform buffer containing shader uniforms
void MonteCarloRTApp::createUniformBuffers()
{
    // Scene uniform
    VkDeviceSize bufferSize = sizeof(UniformData);
    m_sceneBuffer.create(m_vulkanDevice,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        bufferSize);
    CHECK_RESULT(m_sceneBuffer.map());
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
    m_exposureBuffer.create(m_vulkanDevice,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        bufferSize);
    CHECK_RESULT(m_exposureBuffer.map())
    memcpy(m_exposureBuffer.mapped, &m_exposureData, bufferSize);
    m_exposureBuffer.unmap();
}

/*
    Set up a storage image that the ray generation shader will be writing to
*/
void MonteCarloRTApp::createStorageImages()
{
    // NOTE: For the result RGBA32f is used because of the accumulation feature in order to avoid
    // losing quality over frames for "real-time" frames you may want to change the format to a more
    // efficient one, like the swapchain image format "m_swapChain.colorFormat"
    m_storageImage.result.fromNothing(VK_FORMAT_R32G32B32A32_SFLOAT,
        m_width,
        m_height,
        1,
        m_vulkanDevice,
        m_queue,
        VK_FILTER_NEAREST,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_LAYOUT_GENERAL);

    // For normals and depth map 32 bits per channel is less negotiable by the denoiser, maybe 16 if
    // I use Tensor Cores, albedo could be 8 bits but I'll also use 32 to be consistent for now...
    // TODO: to try 8 bits for albedo and 16 bits for depth and normals
    m_storageImage.normalMap.fromNothing(VK_FORMAT_R32G32B32A32_SFLOAT,
        m_width,
        m_height,
        1,
        m_vulkanDevice,
        m_queue,
        VK_FILTER_NEAREST,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_LAYOUT_GENERAL);
    m_storageImage.albedo.fromNothing(VK_FORMAT_R32G32B32A32_SFLOAT,
        m_width,
        m_height,
        1,
        m_vulkanDevice,
        m_queue,
        VK_FILTER_NEAREST,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_LAYOUT_GENERAL);
    m_storageImage.depthMap.fromNothing(VK_FORMAT_R32_SFLOAT,
        m_width,
        m_height,
        1,
        m_vulkanDevice,
        m_queue,
        VK_FILTER_NEAREST,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_LAYOUT_GENERAL);
}

void MonteCarloRTApp::prepare()
{
    BaseRTProject::prepare();
    BaseRTProject::createRTScene("assets/pool/Pool.fbx", m_vertexLayout);

    m_rayTracing = new RayTracingPipeline(m_vulkanDevice, m_rayTracingPipelineProperties);
    m_autoExposure = new AutoExposurePipeline(m_vulkanDevice);

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

void MonteCarloRTApp::render()
{
    if (!m_prepared) {
        return;
    }
    if (BaseRTProject::renderFrame() == VK_SUCCESS) {
        m_sceneUniformData.frameChanged = 0;
        m_sceneUniformData.frameIteration++;
        m_sceneUniformData.frame++;

        // The noise can interfere with the luma calculation to set the manualExposureAdjust, wait
        // for some accumulation:
        if (m_sceneUniformData.frameIteration > 80) {
            // Submit compute commands
            vkWaitForFences(m_device, 1, &m_compute.fence, VK_TRUE, UINT64_MAX);
            vkResetFences(m_device, 1, &m_compute.fence);
            VkSubmitInfo submitInfo {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &m_compute.commandBuffer;
            CHECK_RESULT(vkQueueSubmit(m_compute.queue, 1, &submitInfo, m_compute.fence));
        }
        // ----

        std::cout << '\r' << "| FPS: " << m_lastFps
                  << " -- Sample: " << m_sceneUniformData.frameIteration << " | " << std::flush;
    }
}

MonteCarloRTApp::~MonteCarloRTApp()
{
    delete m_rayTracing;
    delete m_autoExposure;

    vkDestroyPipeline(m_device, m_pipelines.postProcess, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayouts.postProcess, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_postprocessDescriptorSetLayouts.set0Scene, nullptr);
    vkDestroyDescriptorSetLayout(m_device,
        m_postprocessDescriptorSetLayouts.set1InputImage,
        nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_postprocessDescriptorSetLayouts.set2Exposure, nullptr);

    m_storageImage.result.destroy();
    m_storageImage.depthMap.destroy();
    m_storageImage.normalMap.destroy();
    m_storageImage.albedo.destroy();

    m_exposureBuffer.destroy();
    m_sceneBuffer.destroy();
    m_materialsBuffer.destroy();
    m_instancesBuffer.destroy();
    m_lightsBuffer.destroy();

    m_scene->destroy();
}

void MonteCarloRTApp::viewChanged()
{
    auto camera = m_scene->getCamera();
    m_sceneUniformData.projInverse = glm::inverse(camera->matrices.perspective);
    m_sceneUniformData.viewInverse = glm::inverse(camera->matrices.view);
    m_sceneUniformData.frameIteration = 0;
}

void MonteCarloRTApp::onSwapChainRecreation()
{
    // Recreate the result image to fit the new extent size
    m_storageImage.result.destroy();
    m_storageImage.depthMap.destroy();
    m_storageImage.normalMap.destroy();
    m_storageImage.albedo.destroy();
    createStorageImages();
    updateResultImageDescriptorSets();
}

void MonteCarloRTApp::onKeyEvent(int t_key, int t_scancode, int t_action, int t_mods)
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
void MonteCarloRTApp::windowResized()
{
    BaseRTProject::windowResized();
    m_sceneUniformData.frameChanged = 1;
}
