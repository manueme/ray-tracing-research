/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "hybrid_pipeline_ray_tracing.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

HybridPipelineRT::HybridPipelineRT()
    : BaseRTProject("Hybrid Pipeline Ray Tracing", "Hybrid Pipeline Ray Tracing", true)
{
    m_settings.vsync = true;
}

void HybridPipelineRT::getEnabledFeatures() { BaseRTProject::getEnabledFeatures(); }

void HybridPipelineRT::render()
{
    if (!m_prepared) {
        return;
    }
    BaseRTProject::renderFrame();
    std::cout << '\r' << "FPS: " << m_lastFps << std::flush;
}

void HybridPipelineRT::buildCommandBuffers()
{
    VkCommandBufferBeginInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkClearValue clearValues[2];
    clearValues[0].color = m_default_clear_color;
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = m_renderPass;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = m_width;
    renderPassBeginInfo.renderArea.extent.height = m_height;
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues;

    for (int32_t i = 0; i < m_drawCmdBuffers.size(); ++i) {
        renderPassBeginInfo.framebuffer = m_frameBuffers[i];

        VKM_CHECK_RESULT(vkBeginCommandBuffer(m_drawCmdBuffers[i], &cmdBufInfo));

        vkCmdBeginRenderPass(m_drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = initializers::viewport(static_cast<float>(m_width),
            static_cast<float>(m_height),
            0.0f,
            1.0f);
        vkCmdSetViewport(m_drawCmdBuffers[i], 0, 1, &viewport);

        VkRect2D scissor = initializers::rect2D(m_width, m_height, 0, 0);
        vkCmdSetScissor(m_drawCmdBuffers[i], 0, 1, &scissor);

        std::vector<VkDescriptorSet> descriptorSets = { m_descriptorSets.set0Scene[i],
            m_descriptorSets.set1Materials,
            m_descriptorSets.set2Lights };
        vkCmdBindDescriptorSets(m_drawCmdBuffers[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipelineLayout,
            0,
            descriptorSets.size(),
            descriptorSets.data(),
            0,
            nullptr);
        m_scene->draw(m_drawCmdBuffers[i], m_pipelineLayout, vertex_buffer_bind_id);

        vkCmdEndRenderPass(m_drawCmdBuffers[i]);

        VKM_CHECK_RESULT(vkEndCommandBuffer(m_drawCmdBuffers[i]));
    }
}

void HybridPipelineRT::createDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        // Scene uniform buffer
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        // Material array
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
        // Textures
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        // Lights array
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
    };
    uint32_t maxSetsForPool
        = std::max(std::max(m_scene->getMaterialCount(), m_scene->getLightCount()),
            m_scene->getTexturesCount());
    VkDescriptorPoolCreateInfo descriptorPoolInfo
        = initializers::descriptorPoolCreateInfo(poolSizes.size(),
            poolSizes.data(),
            maxSetsForPool);
    VKM_CHECK_RESULT(
        vkCreateDescriptorPool(m_device, &descriptorPoolInfo, nullptr, &m_descriptorPool));
}

void HybridPipelineRT::createDescriptorSetLayout()
{
    // Set 0: Scene matrices
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings
        = { // Binding 0 : Vertex shader uniform buffer
              initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                  0)
          };
    VkDescriptorSetLayoutCreateInfo descriptorLayout
        = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
            setLayoutBindings.size());
    VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_rasterDescriptorSetLayouts.set0Scene));

    // Set 1: Material data
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
    VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_rasterDescriptorSetLayouts.set1Materials));

    // Set 2: Lighting data
    setLayoutBindings.clear();
    // Light list binding 0
    setLayoutBindings.push_back(
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            1));
    descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
        setLayoutBindings.size());
    VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_rasterDescriptorSetLayouts.set2Lights));

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
    VKM_CHECK_RESULT(
        vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout));
}

void HybridPipelineRT::createDescriptorSets()
{
    // Set 0: Scene descriptor
    std::vector<VkDescriptorSetLayout> layouts(m_swapChain.imageCount,
        m_rasterDescriptorSetLayouts.set0Scene);
    VkDescriptorSetAllocateInfo allocInfo
        = initializers::descriptorSetAllocateInfo(m_descriptorPool,
            layouts.data(),
            m_swapChain.imageCount);
    m_descriptorSets.set0Scene.resize(m_swapChain.imageCount);
    VKM_CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &allocInfo, m_descriptorSets.set0Scene.data()));
    for (size_t i = 0; i < m_swapChain.imageCount; i++) {
        std::vector<VkWriteDescriptorSet> writeDescriptorSet0 = {
            // Binding 0 : Vertex shader uniform buffer
            initializers::writeDescriptorSet(m_descriptorSets.set0Scene[i],
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                0,
                &m_sceneBuffers[i].descriptor),
        };
        vkUpdateDescriptorSets(m_device,
            writeDescriptorSet0.size(),
            writeDescriptorSet0.data(),
            0,
            nullptr);
    }

    // Set 1: Material descriptor
    // Materials and Textures descriptor sets
    VkDescriptorSetAllocateInfo textureAllocInfo
        = initializers::descriptorSetAllocateInfo(m_descriptorPool,
            &m_rasterDescriptorSetLayouts.set1Materials,
            1);
    VKM_CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &textureAllocInfo, &m_descriptorSets.set1Materials));

    std::vector<VkDescriptorImageInfo> textureDescriptors;
    for (auto& texture : m_scene->textures) {
        textureDescriptors.push_back(texture.descriptor);
    }

    VkWriteDescriptorSet writeTextureDescriptorSet
        = initializers::writeDescriptorSet(m_descriptorSets.set1Materials,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            0,
            textureDescriptors.data(),
            textureDescriptors.size());
    VkWriteDescriptorSet writeMaterialsDescriptorSet
        = initializers::writeDescriptorSet(m_descriptorSets.set1Materials,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            1,
            &m_materialsBuffer.descriptor);

    std::vector<VkWriteDescriptorSet> writeDescriptorSet1 = {
        writeTextureDescriptorSet,
        writeMaterialsDescriptorSet,
    };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet1.size()),
        writeDescriptorSet1.data(),
        0,
        nullptr);

    // Set 2: Lighting descriptor
    VkDescriptorSetAllocateInfo set2AllocInfo
        = initializers::descriptorSetAllocateInfo(m_descriptorPool,
            &m_rasterDescriptorSetLayouts.set2Lights,
            1);
    VKM_CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &set2AllocInfo, &m_descriptorSets.set2Lights));
    VkWriteDescriptorSet writeLightsDescriptorSet
        = initializers::writeDescriptorSet(m_descriptorSets.set2Lights,
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

void HybridPipelineRT::updateUniformBuffers(uint32_t t_currentImage)
{
    memcpy(m_sceneBuffers[t_currentImage].mapped, &m_sceneUniformData, sizeof(m_sceneUniformData));
}

// Prepare and initialize uniform buffer containing shader uniforms
void HybridPipelineRT::createUniformBuffers()
{
    VkDeviceSize bufferSize = sizeof(m_sceneUniformData);
    m_sceneBuffers.resize(m_swapChain.imageCount);
    for (size_t i = 0; i < m_swapChain.imageCount; i++) {
        m_sceneBuffers[i].create(m_vulkanDevice,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            bufferSize);
        VKM_CHECK_RESULT(m_sceneBuffers[i].map());
    }

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

    // Pipeline for the meshes
    // Load shaders
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    VkGraphicsPipelineCreateInfo pipelineCreateInfo
        = initializers::pipelineCreateInfo(m_pipelineLayout, m_renderPass, 0);
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
            m_vertexLayout.stride(),
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
            sizeof(float) * 9), // Location 2: Texture coordinates
        initializers::vertexInputAttributeDescription(vertex_buffer_bind_id,
            4,
            VK_FORMAT_R32G32B32_SFLOAT,
            sizeof(float) * 11), // Location 3: Color
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
    shaderStages[0] = loadShader("shaders/vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = loadShader("shaders/frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    VKM_CHECK_RESULT(vkCreateGraphicsPipelines(m_device,
        m_pipelineCache,
        1,
        &pipelineCreateInfo,
        nullptr,
        &m_pipelines.raster));
}

void HybridPipelineRT::prepare()
{
    BaseRTProject::prepare();
    BaseRTProject::createRTScene("assets/pool/Pool.fbx", m_vertexLayout, &m_pipelines.raster);

    createUniformBuffers();
    createDescriptorSetLayout();
    createRasterPipeline();
    createDescriptorPool();
    createDescriptorSets();
    buildCommandBuffers();
    m_prepared = true;
}

HybridPipelineRT::~HybridPipelineRT()
{
    // Clean up used Vulkan resources
    // Note : Inherited destructor cleans up resources stored in base class
    vkDestroyPipeline(m_device, m_pipelines.raster, nullptr);

    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rasterDescriptorSetLayouts.set0Scene, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rasterDescriptorSetLayouts.set1Materials, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rasterDescriptorSetLayouts.set2Lights, nullptr);

    for (size_t i = 0; i < m_swapChain.imageCount; i++) {
        m_sceneBuffers[i].destroy();
    }
    m_materialsBuffer.destroy();
    m_lightsBuffer.destroy();

    m_scene->destroy();
}

void HybridPipelineRT::viewChanged()
{
    auto camera = m_scene->getCamera();
    m_sceneUniformData.projection = camera->matrices.perspective;
    m_sceneUniformData.view = camera->matrices.view;
    m_sceneUniformData.model = glm::mat4(1.0f);
    m_sceneUniformData.viewInverse
        = glm::inverseTranspose(m_sceneUniformData.view * m_sceneUniformData.model);
}
