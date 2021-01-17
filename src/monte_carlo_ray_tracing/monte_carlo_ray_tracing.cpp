/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "monte_carlo_ray_tracing.h"

#include "shaders/constants.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

MonteCarloRTApp::MonteCarloRTApp()
    : BaseRTProject("Monte Carlo Ray Tracing", "Monte Carlo Ray Tracing App", true)
{
    m_settings.vsync = false;
}

void MonteCarloRTApp::getEnabledFeatures() { BaseRTProject::getEnabledFeatures(); }

void MonteCarloRTApp::buildCommandBuffers()
{
    VkCommandBufferBeginInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    // Raster pass info
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
        VKM_CHECK_RESULT(vkBeginCommandBuffer(m_drawCmdBuffers[i], &cmdBufInfo));

        /*
            Dispatch the ray tracing commands
        */
        vkCmdBindPipeline(m_drawCmdBuffers[i],
            VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
            m_pipelines.rayTracing);
        std::vector<VkDescriptorSet> rtDescriptorSets
            = { m_rtDescriptorSets.set0AccelerationStructure,
                  m_rtDescriptorSets.set1Scene[i],
                  m_rtDescriptorSets.set2Geometry,
                  m_rtDescriptorSets.set3Materials,
                  m_rtDescriptorSets.set4Lights,
                  m_rtDescriptorSets.set5ResultImage,
                  m_rtDescriptorSets.set6AuxImage };
        vkCmdBindDescriptorSets(m_drawCmdBuffers[i],
            VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
            m_pipelineLayouts.rayTracing,
            0,
            rtDescriptorSets.size(),
            rtDescriptorSets.data(),
            0,
            nullptr);
        vkCmdPushConstants(m_drawCmdBuffers[i],
            m_pipelineLayouts.rayTracing,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
                | VK_SHADER_STAGE_MISS_BIT_KHR,
            0,
            sizeof(PathTracerParameters),
            &m_pathTracerParams);

        // Calculate shader bindings
        // TODO: use separated binding tables for rayGen miss and hit
        const uint32_t handleSizeAligned
            = tools::alignedSize(m_rayTracingPipelineProperties.shaderGroupHandleSize,
                m_rayTracingPipelineProperties.shaderGroupHandleAlignment);
        VkStridedDeviceAddressRegionKHR rayGenSbtRegion;
        rayGenSbtRegion.deviceAddress = m_shaderBindingTable.getDeviceAddress();
        rayGenSbtRegion.stride = handleSizeAligned;
        rayGenSbtRegion.size = handleSizeAligned;
        VkStridedDeviceAddressRegionKHR missSbtRegion = rayGenSbtRegion;
        VkStridedDeviceAddressRegionKHR hitSbtRegion = rayGenSbtRegion;
        VkStridedDeviceAddressRegionKHR emptySbtEntry = {};

        vkCmdTraceRaysKHR(m_drawCmdBuffers[i],
            &rayGenSbtRegion,
            &missSbtRegion,
            &hitSbtRegion,
            &emptySbtEntry,
            m_width,
            m_height,
            1);

        // Raster section:
        // The render pass, viewport and scissor is defined only for the raster
        // pipeline
        renderPassBeginInfo.framebuffer = m_frameBuffers[i];
        vkCmdBeginRenderPass(m_drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = initializers::viewport(static_cast<float>(m_width),
            static_cast<float>(m_height),
            0.0f,
            1.0f);
        vkCmdSetViewport(m_drawCmdBuffers[i], 0, 1, &viewport);
        VkRect2D scissor = initializers::rect2D(m_width, m_height, 0, 0);
        vkCmdSetScissor(m_drawCmdBuffers[i], 0, 1, &scissor);
        std::vector<VkDescriptorSet> rasterDescriptorSets = { m_rasterDescriptorSets.set0Scene[i],
            m_rasterDescriptorSets.set1InputImage,
            m_rasterDescriptorSets.set2ConvolutionKernels };
        vkCmdBindDescriptorSets(m_drawCmdBuffers[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipelineLayouts.raster,
            0,
            rasterDescriptorSets.size(),
            rasterDescriptorSets.data(),
            0,
            nullptr);
        auto aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);
        vkCmdBindPipeline(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.raster);
        vkCmdPushConstants(m_drawCmdBuffers[i],
            m_pipelineLayouts.raster,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(float),
            &aspectRatio);
        vkCmdDraw(m_drawCmdBuffers[i], 3, 1, 0, 0);

        vkCmdEndRenderPass(m_drawCmdBuffers[i]);
        // End of raster section --

        VKM_CHECK_RESULT(vkEndCommandBuffer(m_drawCmdBuffers[i]));
    }
}

void MonteCarloRTApp::createDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
        // Scene description
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        // Vertex, Index and Material Indexes
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
        // Textures
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        // Material array
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
        // Lights array
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
        // Result images
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        // Aux images
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        // Convolution Kernel
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
    };
    uint32_t maxSetsForPool
        = std::max(std::max(m_scene->getMaterialCount(), m_scene->getLightCount()),
            m_scene->getTexturesCount());
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo
        = initializers::descriptorPoolCreateInfo(poolSizes.size(),
            poolSizes.data(),
            maxSetsForPool);
    VKM_CHECK_RESULT(
        vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool));
}

void MonteCarloRTApp::createDescriptorSetsLayout()
{
    // Set 0: Acceleration Structure Layout
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        // Binding 0 : Acceleration structure
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
                | VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            0),
    };
    VkDescriptorSetLayoutCreateInfo descriptorLayout
        = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
            setLayoutBindings.size());
    VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_rtDescriptorSetLayouts.set0AccelerationStructure));

    // Set 1: Scene matrices
    setLayoutBindings.clear();
    setLayoutBindings = {
        // Binding 0 : Scene uniform buffer
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
                | VK_SHADER_STAGE_MISS_BIT_KHR,
            0),
    };
    descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
        setLayoutBindings.size());
    VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_rtDescriptorSetLayouts.set1Scene));

    // Set 2: Geometry data
    setLayoutBindings.clear();
    setLayoutBindings = {
        // Binding 0 : Vertex uniform buffer
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            0),
        // Binding 1 : Vertex Index uniform buffer
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            1),
        // Binding 2 : Instance Information uniform buffer
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            2),
    };
    descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
        setLayoutBindings.size());
    VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_rtDescriptorSetLayouts.set2Geometry));

    // Set 3: Textures data
    setLayoutBindings.clear();
    // Texture list binding 0
    setLayoutBindings.push_back(
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            0,
            m_scene->textures.size()));
    // Material list binding 1
    setLayoutBindings.push_back(
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            1));
    descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
        setLayoutBindings.size());
    VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_rtDescriptorSetLayouts.set3Materials));

    // Set 4: Lighting data
    setLayoutBindings.clear();
    // Light list binding 0
    setLayoutBindings.push_back(
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
            0,
            1));
    descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
        setLayoutBindings.size());
    VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_rtDescriptorSetLayouts.set4Lights));

    // Set 5: Result Image
    setLayoutBindings.clear();
    setLayoutBindings.push_back(
        // Binding 0 : Result Image Color
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            0));
    setLayoutBindings.push_back(
        // Binding 1 : Result Image Depth Map
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            1));
    setLayoutBindings.push_back(
        // Binding 2 : Result Image
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            2));

    descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
        setLayoutBindings.size());
    VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_rtDescriptorSetLayouts.set5ResultImage));

    // Set 6: Aux Images
    setLayoutBindings.clear();
    setLayoutBindings.push_back(
        // Binding 0 : RBG Noise Image
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
            0));
    descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
        setLayoutBindings.size());
    VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_rtDescriptorSetLayouts.set6AuxImage));

    // Ray Tracing Pipeline Layout
    // Push constant to pass path tracer parameters
    VkPushConstantRange rtPushConstantRange
        = initializers::pushConstantRange(VK_SHADER_STAGE_RAYGEN_BIT_KHR
                | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
            sizeof(PathTracerParameters),
            0);
    std::array<VkDescriptorSetLayout, 7> rayTracingSetLayouts
        = { m_rtDescriptorSetLayouts.set0AccelerationStructure,
              m_rtDescriptorSetLayouts.set1Scene,
              m_rtDescriptorSetLayouts.set2Geometry,
              m_rtDescriptorSetLayouts.set3Materials,
              m_rtDescriptorSetLayouts.set4Lights,
              m_rtDescriptorSetLayouts.set5ResultImage,
              m_rtDescriptorSetLayouts.set6AuxImage };

    VkPipelineLayoutCreateInfo rayTracingPipelineLayoutCreateInfo
        = initializers::pipelineLayoutCreateInfo(rayTracingSetLayouts.data(),
            rayTracingSetLayouts.size());
    rayTracingPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    rayTracingPipelineLayoutCreateInfo.pPushConstantRanges = &rtPushConstantRange;
    VKM_CHECK_RESULT(vkCreatePipelineLayout(m_device,
        &rayTracingPipelineLayoutCreateInfo,
        nullptr,
        &m_pipelineLayouts.rayTracing));

    // Set 0 Raster: Scene information buffer
    setLayoutBindings.clear();
    setLayoutBindings.push_back(
        // Binding 0 : Buffer
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0));
    descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
        setLayoutBindings.size());
    VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_rasterDescriptorSetLayouts.set0Scene));

    // Set 1 Raster: Input Image
    setLayoutBindings.clear();
    setLayoutBindings.push_back(
        // Binding 0 : Result Image Color
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0));
    setLayoutBindings.push_back(
        // Binding 1 : Result Image Depth Map
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            1));
    setLayoutBindings.push_back(
        // Binding 2 : Result Image First Sample
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            2));
    descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
        setLayoutBindings.size());
    VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_rasterDescriptorSetLayouts.set1InputImage));

    // Set 2 Raster: Convolution Kernel
    setLayoutBindings.clear();
    setLayoutBindings.push_back(
        // Binding 0 : Layer 1
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0));
    descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
        setLayoutBindings.size());
    VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_rasterDescriptorSetLayouts.set2ConvolutionKernels));

    // Raster Pipeline Layout
    // Push constant to pass camera aspect ratio
    VkPushConstantRange pushConstantRange
        = initializers::pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(float), 0);
    std::array<VkDescriptorSetLayout, 3> rasterSetLayouts
        = { m_rasterDescriptorSetLayouts.set0Scene,
              m_rasterDescriptorSetLayouts.set1InputImage,
              m_rasterDescriptorSetLayouts.set2ConvolutionKernels };
    VkPipelineLayoutCreateInfo rasterPipelineLayoutCreateInfo
        = initializers::pipelineLayoutCreateInfo(rasterSetLayouts.data(), rasterSetLayouts.size());
    rasterPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    rasterPipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    VKM_CHECK_RESULT(vkCreatePipelineLayout(m_device,
        &rasterPipelineLayoutCreateInfo,
        nullptr,
        &m_pipelineLayouts.raster));
}

void MonteCarloRTApp::createRasterPipeline()
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
    VkPipelineMultisampleStateCreateInfo multisampleState
        = initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
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

    // Pipeline for the meshes
    // Load shaders
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    shaderStages[0] = loadShader("./shaders/final.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = loadShader("./shaders/final.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VkGraphicsPipelineCreateInfo pipelineCreateInfo
        = initializers::pipelineCreateInfo(m_pipelineLayouts.raster, m_renderPass, 0);
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
    VKM_CHECK_RESULT(vkCreateGraphicsPipelines(m_device,
        m_pipelineCache,
        1,
        &pipelineCreateInfo,
        nullptr,
        &m_pipelines.raster));
}

void MonteCarloRTApp::createRTPipeline()
{
    std::array<VkPipelineShaderStageCreateInfo, 6> shaderStages {};
    shaderStages[SBT_MC_RAY_GEN_INDEX]
        = loadShader("./shaders/raygen.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    shaderStages[SBT_MC_MISS_INDEX]
        = loadShader("./shaders/miss.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR);
    shaderStages[SBT_MC_SHADOW_MISS_INDEX]
        = loadShader("./shaders/shadow.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR);
    shaderStages[SBT_MC_ANY_HIT_INDEX]
        = loadShader("./shaders/anyhit.rahit.spv", VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
    shaderStages[SBT_MC_CLOSEST_HIT_INDEX]
        = loadShader("./shaders/closesthit.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    shaderStages[SBT_MC_SHADOW_ANY_HIT_INDEX]
        = loadShader("./shaders/shadow.rahit.spv", VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
    /*
      Setup ray tracing shader groups
    */
    std::array<VkRayTracingShaderGroupCreateInfoKHR, SBT_MC_NUM_SHADER_GROUPS> groups {};
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
    groups[SBT_MC_RAY_GEN_GROUP].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[SBT_MC_RAY_GEN_GROUP].generalShader = SBT_MC_RAY_GEN_INDEX;
    // Scene miss shader group
    groups[SBT_MC_MISS_GROUP].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[SBT_MC_MISS_GROUP].generalShader = SBT_MC_MISS_INDEX;
    // Shadow miss shader group
    groups[SBT_MC_SHADOW_MISS_GROUP].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[SBT_MC_SHADOW_MISS_GROUP].generalShader = SBT_MC_SHADOW_MISS_INDEX;
    // Any hit shader group and closest hit shader group
    groups[SBT_MC_HIT_GROUP].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groups[SBT_MC_HIT_GROUP].anyHitShader = SBT_MC_ANY_HIT_INDEX;
    groups[SBT_MC_HIT_GROUP].closestHitShader = SBT_MC_CLOSEST_HIT_INDEX;
    // Shadow closest hit shader group
    groups[SBT_MC_SHADOW_HIT_GROUP].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groups[SBT_MC_SHADOW_HIT_GROUP].anyHitShader = SBT_MC_SHADOW_ANY_HIT_INDEX;

    VkRayTracingPipelineCreateInfoKHR rayPipelineInfo {};
    rayPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    rayPipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    rayPipelineInfo.pStages = shaderStages.data();
    rayPipelineInfo.groupCount = static_cast<uint32_t>(groups.size());
    rayPipelineInfo.pGroups = groups.data();
    rayPipelineInfo.maxPipelineRayRecursionDepth = m_pathTracerParams.maxDepth;
    rayPipelineInfo.layout = m_pipelineLayouts.rayTracing;
    VKM_CHECK_RESULT(vkCreateRayTracingPipelinesKHR(m_device,
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        1,
        &rayPipelineInfo,
        nullptr,
        &m_pipelines.rayTracing));
}

void MonteCarloRTApp::assignPushConstants()
{
    m_pathTracerParams = {
        m_ray_tracer_depth, // Max depth
        m_ray_tracer_samples, // samples per frame
    };
}

void MonteCarloRTApp::createDescriptorSets()
{
    // Set 0: Acceleration Structure descriptor
    VkDescriptorSetAllocateInfo set0AllocInfo
        = initializers::descriptorSetAllocateInfo(m_descriptorPool,
            &m_rtDescriptorSetLayouts.set0AccelerationStructure,
            1);
    VKM_CHECK_RESULT(vkAllocateDescriptorSets(m_device,
        &set0AllocInfo,
        &m_rtDescriptorSets.set0AccelerationStructure));

    VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo {};
    descriptorAccelerationStructureInfo.sType
        = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    auto tlasHandle = m_topLevelAS.getHandle();
    descriptorAccelerationStructureInfo.pAccelerationStructures = &tlasHandle;

    VkWriteDescriptorSet accelerationStructureWrite {};
    accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    // The specialized acceleration structure descriptor has to be chained
    accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
    accelerationStructureWrite.dstSet = m_rtDescriptorSets.set0AccelerationStructure;
    accelerationStructureWrite.dstBinding = 0;
    accelerationStructureWrite.descriptorCount = 1;
    accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    std::vector<VkWriteDescriptorSet> writeDescriptorSet0 = { accelerationStructureWrite };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet0.size()),
        writeDescriptorSet0.data(),
        0,
        VK_NULL_HANDLE);

    // Set 1: Scene descriptor
    std::vector<VkDescriptorSetLayout> set1Layouts(m_swapChain.imageCount,
        m_rtDescriptorSetLayouts.set1Scene);
    VkDescriptorSetAllocateInfo set1AllocInfo
        = initializers::descriptorSetAllocateInfo(m_descriptorPool,
            set1Layouts.data(),
            m_swapChain.imageCount);
    m_rtDescriptorSets.set1Scene.resize(m_swapChain.imageCount);
    VKM_CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &set1AllocInfo, m_rtDescriptorSets.set1Scene.data()));
    for (size_t i = 0; i < m_swapChain.imageCount; i++) {
        VkWriteDescriptorSet uniformBufferWrite
            = initializers::writeDescriptorSet(m_rtDescriptorSets.set1Scene[i],
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                0,
                &m_sceneBuffers[i].descriptor);
        std::vector<VkWriteDescriptorSet> writeDescriptorSet1 = { uniformBufferWrite };
        vkUpdateDescriptorSets(m_device,
            static_cast<uint32_t>(writeDescriptorSet1.size()),
            writeDescriptorSet1.data(),
            0,
            VK_NULL_HANDLE);
    }

    // Set 2: Geometry descriptor
    VkDescriptorSetAllocateInfo set2AllocInfo
        = initializers::descriptorSetAllocateInfo(m_descriptorPool,
            &m_rtDescriptorSetLayouts.set2Geometry,
            1);
    VKM_CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &set2AllocInfo, &m_rtDescriptorSets.set2Geometry));

    VkDescriptorBufferInfo vertexBufferDescriptor {};
    vertexBufferDescriptor.buffer = m_scene->vertices.buffer;
    vertexBufferDescriptor.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo indexBufferDescriptor {};
    indexBufferDescriptor.buffer = m_scene->indices.buffer;
    indexBufferDescriptor.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet vertexBufferWrite
        = initializers::writeDescriptorSet(m_rtDescriptorSets.set2Geometry,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            0,
            &vertexBufferDescriptor);
    VkWriteDescriptorSet indexBufferWrite
        = initializers::writeDescriptorSet(m_rtDescriptorSets.set2Geometry,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            1,
            &indexBufferDescriptor);
    VkWriteDescriptorSet materialIndexBufferWrite
        = initializers::writeDescriptorSet(m_rtDescriptorSets.set2Geometry,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            2,
            &m_instancesBuffer.descriptor);
    std::vector<VkWriteDescriptorSet> writeDescriptorSet2
        = { vertexBufferWrite, indexBufferWrite, materialIndexBufferWrite };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet2.size()),
        writeDescriptorSet2.data(),
        0,
        VK_NULL_HANDLE);

    // Set 3: Materials and Textures descriptor
    VkDescriptorSetAllocateInfo set3AllocInfo
        = initializers::descriptorSetAllocateInfo(m_descriptorPool,
            &m_rtDescriptorSetLayouts.set3Materials,
            1);
    VKM_CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &set3AllocInfo, &m_rtDescriptorSets.set3Materials));

    std::vector<VkDescriptorImageInfo> textureDescriptors;
    for (auto& texture : m_scene->textures) {
        textureDescriptors.push_back(texture.descriptor);
    }

    VkWriteDescriptorSet writeTextureDescriptorSet
        = initializers::writeDescriptorSet(m_rtDescriptorSets.set3Materials,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            0,
            textureDescriptors.data(),
            textureDescriptors.size());
    VkWriteDescriptorSet writeMaterialsDescriptorSet
        = initializers::writeDescriptorSet(m_rtDescriptorSets.set3Materials,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            1,
            &m_materialsBuffer.descriptor);

    std::vector<VkWriteDescriptorSet> writeDescriptorSet3
        = { writeTextureDescriptorSet, writeMaterialsDescriptorSet };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet3.size()),
        writeDescriptorSet3.data(),
        0,
        nullptr);

    // Set 4: Lighting descriptor
    VkDescriptorSetAllocateInfo set4AllocInfo
        = initializers::descriptorSetAllocateInfo(m_descriptorPool,
            &m_rtDescriptorSetLayouts.set4Lights,
            1);
    VKM_CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &set4AllocInfo, &m_rtDescriptorSets.set4Lights));
    VkWriteDescriptorSet writeLightsDescriptorSet
        = initializers::writeDescriptorSet(m_rtDescriptorSets.set4Lights,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            0,
            &m_lightsBuffer.descriptor);

    std::vector<VkWriteDescriptorSet> writeDescriptorSet4 = { writeLightsDescriptorSet };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet4.size()),
        writeDescriptorSet4.data(),
        0,
        nullptr);

    // Set 5: Result image descriptor
    VkDescriptorSetAllocateInfo set5AllocInfo
        = initializers::descriptorSetAllocateInfo(m_descriptorPool,
            &m_rtDescriptorSetLayouts.set5ResultImage,
            1);
    VKM_CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &set5AllocInfo, &m_rtDescriptorSets.set5ResultImage));

    // Set 6: Aux image descriptor
    VkDescriptorSetAllocateInfo set6AllocInfo
        = initializers::descriptorSetAllocateInfo(m_descriptorPool,
            &m_rtDescriptorSetLayouts.set6AuxImage,
            1);
    VKM_CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &set6AllocInfo, &m_rtDescriptorSets.set6AuxImage));
    VkWriteDescriptorSet auxImagesRGBNoiseWrite
        = initializers::writeDescriptorSet(m_rtDescriptorSets.set6AuxImage,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            0,
            &m_auxImages.rgbNoise.descriptor);
    std::vector<VkWriteDescriptorSet> writeDescriptorSet6 = { auxImagesRGBNoiseWrite };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet6.size()),
        writeDescriptorSet6.data(),
        0,
        VK_NULL_HANDLE);

    // Raster input scene descriptor set, set 0
    std::vector<VkDescriptorSetLayout> rasterSet0Layouts(m_swapChain.imageCount,
        m_rasterDescriptorSetLayouts.set0Scene);
    VkDescriptorSetAllocateInfo allocInfo
        = initializers::descriptorSetAllocateInfo(m_descriptorPool,
            rasterSet0Layouts.data(),
            m_swapChain.imageCount);
    m_rasterDescriptorSets.set0Scene.resize(m_swapChain.imageCount);
    VKM_CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &allocInfo, m_rasterDescriptorSets.set0Scene.data()));
    for (size_t i = 0; i < m_swapChain.imageCount; i++) {
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

    // Raster input image descriptor set, set 1
    VkDescriptorSetAllocateInfo inputImageAllocateInfo
        = initializers::descriptorSetAllocateInfo(m_descriptorPool,
            &m_rasterDescriptorSetLayouts.set1InputImage,
            1);
    VKM_CHECK_RESULT(vkAllocateDescriptorSets(m_device,
        &inputImageAllocateInfo,
        &m_rasterDescriptorSets.set1InputImage));

    // Raster input image descriptor set, set 2
    VkDescriptorSetAllocateInfo convolutionKernelAllocateInfo
        = initializers::descriptorSetAllocateInfo(m_descriptorPool,
            &m_rasterDescriptorSetLayouts.set2ConvolutionKernels,
            1);
    VKM_CHECK_RESULT(vkAllocateDescriptorSets(m_device,
        &convolutionKernelAllocateInfo,
        &m_rasterDescriptorSets.set2ConvolutionKernels));
    VkWriteDescriptorSet convolutionKernelsWrite
        = initializers::writeDescriptorSet(m_rasterDescriptorSets.set2ConvolutionKernels,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            0,
            &m_convolutionKernels.layer1.descriptor);
    std::vector<VkWriteDescriptorSet> writeDescriptorSet2Raster = { convolutionKernelsWrite };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet2Raster.size()),
        writeDescriptorSet2Raster.data(),
        0,
        VK_NULL_HANDLE);

    updateResultImageDescriptorSets();
}

void MonteCarloRTApp::updateResultImageDescriptorSets()
{
    VkWriteDescriptorSet resultImageWrite
        = initializers::writeDescriptorSet(m_rtDescriptorSets.set5ResultImage,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            0,
            &m_storageImage.color.descriptor);
    VkWriteDescriptorSet resultDepthMapWrite
        = initializers::writeDescriptorSet(m_rtDescriptorSets.set5ResultImage,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            1,
            &m_storageImage.depthMap.descriptor);
    VkWriteDescriptorSet resultFirstSampleWrite
        = initializers::writeDescriptorSet(m_rtDescriptorSets.set5ResultImage,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            2,
            &m_storageImage.firstSample.descriptor);
    std::vector<VkWriteDescriptorSet> writeDescriptorSet5
        = { resultImageWrite, resultDepthMapWrite, resultFirstSampleWrite };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet5.size()),
        writeDescriptorSet5.data(),
        0,
        VK_NULL_HANDLE);

    VkWriteDescriptorSet inputImageWrite
        = initializers::writeDescriptorSet(m_rasterDescriptorSets.set1InputImage,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            0,
            &m_storageImage.color.descriptor);
    VkWriteDescriptorSet inputDepthMapWrite
        = initializers::writeDescriptorSet(m_rasterDescriptorSets.set1InputImage,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            &m_storageImage.depthMap.descriptor);
    VkWriteDescriptorSet inputFirstSampleWrite
        = initializers::writeDescriptorSet(m_rasterDescriptorSets.set1InputImage,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            2,
            &m_storageImage.firstSample.descriptor);
    std::vector<VkWriteDescriptorSet> writeDescriptorSet1Raster
        = { inputImageWrite, inputDepthMapWrite, inputFirstSampleWrite };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet1Raster.size()),
        writeDescriptorSet1Raster.data(),
        0,
        VK_NULL_HANDLE);
}

void MonteCarloRTApp::updateUniformBuffers(uint32_t t_currentImage)
{
    memcpy(m_sceneBuffers[t_currentImage].mapped, &m_sceneUniformData, sizeof(UniformData));
}

// Prepare and initialize uniform buffer containing shader uniforms
void MonteCarloRTApp::createUniformBuffers()
{
    // Scene uniform
    VkDeviceSize bufferSize = sizeof(UniformData);
    m_sceneBuffers.resize(m_swapChain.imageCount);
    for (size_t i = 0; i < m_swapChain.imageCount; i++) {
        m_sceneBuffers[i].create(m_vulkanDevice,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            bufferSize);
        VKM_CHECK_RESULT(m_sceneBuffers[i].map());
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
}

/*
    Set up a storage image that the ray generation shader will be writing to
*/
void MonteCarloRTApp::createStorageImages()
{
    // Unused
    m_auxImages.rgbNoise.loadFromFile("assets/other/rgbNoise.png",
        VK_FORMAT_B8G8R8A8_UNORM,
        m_vulkanDevice,
        m_queue,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        false);

    m_storageImage.firstSample.fromNothing(m_swapChain.colorFormat,
        m_width,
        m_height,
        m_vulkanDevice,
        m_queue,
        VK_FILTER_NEAREST,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_LAYOUT_GENERAL);
    // NOTE: I RGBA32f is used because of the accumulation feature in order to avoid losing quality
    // over frames for "real-time" frames you may want to change the format to a more efficient one,
    // like the swapchain image format "m_swapChain.colorFormat"
    m_storageImage.color.fromNothing(VK_FORMAT_R32G32B32A32_SFLOAT,
        m_width,
        m_height,
        m_vulkanDevice,
        m_queue,
        VK_FILTER_NEAREST,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_LAYOUT_GENERAL);
    m_storageImage.depthMap.fromNothing(VK_FORMAT_R32_SFLOAT,
        m_width,
        m_height,
        m_vulkanDevice,
        m_queue,
        VK_FILTER_NEAREST,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_LAYOUT_GENERAL);
}

void MonteCarloRTApp::createConvolutionKernels()
{
    const auto w = 5, h = 5, bpp = 32;
    const auto size = w * h * bpp;
    std::allocator<glm::float32_t> alloc;
    glm::float32_t* kernel = alloc.allocate(size);
    kernel[0] = 0.00390625f;
    kernel[1] = 0.015625f;
    kernel[2] = 0.0234375f;
    kernel[3] = 0.015625f;
    kernel[4] = 0.00390625f;

    kernel[5] = 0.015625f;
    kernel[6] = 0.0625f;
    kernel[7] = 0.09375f;
    kernel[8] = 0.0625f;
    kernel[9] = 0.015625f;

    kernel[10] = 0.0234375f;
    kernel[11] = 0.09375f;
    kernel[12] = 0.140625f;
    kernel[13] = 0.09375f;
    kernel[14] = 0.0234375f;

    kernel[15] = 0.015625f;
    kernel[16] = 0.0625f;
    kernel[17] = 0.09375f;
    kernel[18] = 0.0625f;
    kernel[19] = 0.015625f;

    kernel[20] = 0.00390625f;
    kernel[21] = 0.015625f;
    kernel[22] = 0.0234375f;
    kernel[23] = 0.015625f;
    kernel[24] = 0.00390625f;

    m_convolutionKernels.layer1.fromBuffer(kernel,
        size,
        VK_FORMAT_R32_SFLOAT,
        w,
        h,
        m_vulkanDevice,
        m_queue,
        VK_FILTER_NEAREST,
        VK_IMAGE_USAGE_STORAGE_BIT,
        VK_IMAGE_LAYOUT_GENERAL);
    alloc.destroy(kernel);
}

void MonteCarloRTApp::createShaderRTBindingTable()
{
    // Create buffer for the shader binding table
    const uint32_t sbtSize
        = m_rayTracingPipelineProperties.shaderGroupHandleSize * SBT_MC_NUM_SHADER_GROUPS;
    m_shaderBindingTable.create(m_vulkanDevice,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        sbtSize);
    m_shaderBindingTable.map();
    auto shaderHandleStorage = new uint8_t[sbtSize];
    // Get shader identifiers
    VKM_CHECK_RESULT(vkGetRayTracingShaderGroupHandlesKHR(m_device,
        m_pipelines.rayTracing,
        0,
        SBT_MC_NUM_SHADER_GROUPS,
        sbtSize,
        shaderHandleStorage));
    auto* data = static_cast<uint8_t*>(m_shaderBindingTable.mapped);
    // Copy the shader identifiers to the shader binding table
    data += BaseRTProject::copyRTShaderIdentifier(data, shaderHandleStorage, SBT_MC_RAY_GEN_GROUP);
    data += BaseRTProject::copyRTShaderIdentifier(data, shaderHandleStorage, SBT_MC_MISS_GROUP);
    data += BaseRTProject::copyRTShaderIdentifier(data,
        shaderHandleStorage,
        SBT_MC_SHADOW_MISS_GROUP);
    data += BaseRTProject::copyRTShaderIdentifier(data, shaderHandleStorage, SBT_MC_HIT_GROUP);
    data += BaseRTProject::copyRTShaderIdentifier(data,
        shaderHandleStorage,
        SBT_MC_SHADOW_HIT_GROUP);
    m_shaderBindingTable.unmap();
}

void MonteCarloRTApp::prepare()
{
    BaseRTProject::prepare();
    BaseRTProject::createRTScene("assets/pool/Pool.fbx", m_vertexLayout);

    createStorageImages();
    createConvolutionKernels();
    createUniformBuffers();
    createDescriptorSetsLayout();
    assignPushConstants();
    createRasterPipeline();
    createRTPipeline();
    createShaderRTBindingTable();
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
    m_sceneUniformData.frameIteration++;
    m_sceneUniformData.frame++;
    m_sceneUniformData.frameChanged = 0;
    BaseRTProject::renderFrame();
    std::cout << '\r' << "FPS: " << m_lastFps << std::flush;
}

MonteCarloRTApp::~MonteCarloRTApp()
{
    vkDestroyPipeline(m_device, m_pipelines.raster, nullptr);
    vkDestroyPipeline(m_device, m_pipelines.rayTracing, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayouts.raster, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayouts.rayTracing, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rasterDescriptorSetLayouts.set0Scene, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rasterDescriptorSetLayouts.set1InputImage, nullptr);
    vkDestroyDescriptorSetLayout(m_device,
        m_rasterDescriptorSetLayouts.set2ConvolutionKernels,
        nullptr);
    vkDestroyDescriptorSetLayout(m_device,
        m_rtDescriptorSetLayouts.set0AccelerationStructure,
        nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rtDescriptorSetLayouts.set1Scene, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rtDescriptorSetLayouts.set2Geometry, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rtDescriptorSetLayouts.set3Materials, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rtDescriptorSetLayouts.set4Lights, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rtDescriptorSetLayouts.set5ResultImage, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rtDescriptorSetLayouts.set6AuxImage, nullptr);
    m_storageImage.color.destroy();
    m_storageImage.depthMap.destroy();
    m_storageImage.firstSample.destroy();
    m_auxImages.rgbNoise.destroy();
    m_convolutionKernels.layer1.destroy();

    m_shaderBindingTable.destroy();
    for (size_t i = 0; i < m_swapChain.imageCount; i++) {
        m_sceneBuffers[i].destroy();
    }
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
    m_storageImage.firstSample.destroy();
    m_storageImage.color.destroy();
    m_storageImage.depthMap.destroy();
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
    default:
        break;
    }
}
void MonteCarloRTApp::windowResized()
{
    BaseRTProject::windowResized();
    m_sceneUniformData.frameChanged = 1;
}
