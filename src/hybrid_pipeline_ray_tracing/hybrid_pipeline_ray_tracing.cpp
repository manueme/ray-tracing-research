/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "hybrid_pipeline_ray_tracing.h"
#include "shaders/constants.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

HybridPipelineRT::HybridPipelineRT()
    : BaseRTProject("Hybrid Pipeline Ray Tracing", "Hybrid Pipeline Ray Tracing", true)
{
    m_settings.vsync = false;
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

    std::array<VkClearValue, 8> rasterClearValues = {};
    rasterClearValues[0].color = m_default_clear_color;
    rasterClearValues[1].color = m_default_clear_color;
    rasterClearValues[2].color = m_default_clear_color;
    rasterClearValues[3].color = m_default_clear_color;
    rasterClearValues[4].color = m_default_clear_color;
    rasterClearValues[5].color = m_default_clear_color;
    rasterClearValues[6].depthStencil = { 1.0f, 0 };
    rasterClearValues[7].depthStencil = { 1.0f, 0 };
    VkRenderPassBeginInfo rasterPassBeginInfo = {};
    rasterPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rasterPassBeginInfo.renderPass = m_offscreenRenderPass;
    rasterPassBeginInfo.renderArea.offset.x = 0;
    rasterPassBeginInfo.renderArea.offset.y = 0;
    rasterPassBeginInfo.renderArea.extent.width = m_width;
    rasterPassBeginInfo.renderArea.extent.height = m_height;
    rasterPassBeginInfo.clearValueCount = static_cast<uint32_t>(rasterClearValues.size());
    rasterPassBeginInfo.pClearValues = rasterClearValues.data();

    std::array<VkClearValue, 2> postprocessClearValues = {};
    rasterClearValues[0].color = m_default_clear_color;
    rasterClearValues[1].depthStencil = { 1.0f, 0 };
    VkRenderPassBeginInfo postprocessPassBeginInfo = {};
    postprocessPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    postprocessPassBeginInfo.renderPass = m_renderPass;
    postprocessPassBeginInfo.renderArea.offset.x = 0;
    postprocessPassBeginInfo.renderArea.offset.y = 0;
    postprocessPassBeginInfo.renderArea.extent.width = m_width;
    postprocessPassBeginInfo.renderArea.extent.height = m_height;
    postprocessPassBeginInfo.clearValueCount = static_cast<uint32_t>(postprocessClearValues.size());
    postprocessPassBeginInfo.pClearValues = postprocessClearValues.data();

    for (int32_t i = 0; i < m_drawCmdBuffers.size(); ++i) {
        rasterPassBeginInfo.framebuffer = m_offscreenFramebuffers[i];

        VKM_CHECK_RESULT(vkBeginCommandBuffer(m_drawCmdBuffers[i], &cmdBufInfo))

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
        vkCmdBindPipeline(m_drawCmdBuffers[i],
            VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
            m_pipelines.rayTracing);
        std::vector<VkDescriptorSet> rtDescriptorSets
            = { m_rtDescriptorSets.set0AccelerationStructure,
                  m_rtDescriptorSets.set1Scene[i],
                  m_rtDescriptorSets.set2Geometry,
                  m_rtDescriptorSets.set3Materials,
                  m_rtDescriptorSets.set4Lights,
                  m_rtDescriptorSets.set5OffscreenImages[i],
                  m_rtDescriptorSets.set6StorageImages[i] };
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
        // End Ray tracing "pass"

        // Postprocess section:
        postprocessPassBeginInfo.framebuffer = m_frameBuffers[i];
        vkCmdBeginRenderPass(m_drawCmdBuffers[i],
            &postprocessPassBeginInfo,
            VK_SUBPASS_CONTENTS_INLINE);

        viewport = initializers::viewport(static_cast<float>(m_width),
            static_cast<float>(m_height),
            0.0f,
            1.0f);
        vkCmdSetViewport(m_drawCmdBuffers[i], 0, 1, &viewport);
        scissor = initializers::rect2D(m_width, m_height, 0, 0);
        vkCmdSetScissor(m_drawCmdBuffers[i], 0, 1, &scissor);
        std::vector<VkDescriptorSet> postprocessDescriptorSets
            = { m_postProcessDescriptorSets.set0StorageImages[i] };
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

        VKM_CHECK_RESULT(vkEndCommandBuffer(m_drawCmdBuffers[i]))
    }
}

void HybridPipelineRT::createDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
        // Scene uniform buffer
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        // Vertex, Index and Material Indexes
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
        // Textures
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        // Material array
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
        // Lights array
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
        // Offscreen images
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        // Storage images
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
    };
    // Calculate max set for pool
    const auto asSets = 1;
    const auto sceneSets = 1 * m_swapChain.imageCount;
    const auto vertexAndIndexes = 3;
    const auto textures = 1;
    const auto materials = 1;
    const auto lights = 1;
    const auto offscreenImages = 6 * m_swapChain.imageCount;
    const auto storageImages = 1 * m_swapChain.imageCount;
    uint32_t maxSetsForPool = asSets + sceneSets + vertexAndIndexes + textures + materials + lights
        + offscreenImages + storageImages;
    // ---
    VkDescriptorPoolCreateInfo descriptorPoolInfo
        = initializers::descriptorPoolCreateInfo(poolSizes.size(),
            poolSizes.data(),
            maxSetsForPool);
    VKM_CHECK_RESULT(
        vkCreateDescriptorPool(m_device, &descriptorPoolInfo, nullptr, &m_descriptorPool))
}

void HybridPipelineRT::createDescriptorSetLayout()
{
    // **** RASTERIZATION LAYOUTS ****
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
        VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
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
        VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
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
        VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
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
        VKM_CHECK_RESULT(vkCreatePipelineLayout(m_device,
            &pipelineLayoutCreateInfo,
            nullptr,
            &m_pipelineLayouts.raster))
    }
    // **** END RASTERIZATION LAYOUTS ****
    // **** RAY TRACING LAYOUTS ****
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
            &m_rtDescriptorSetLayouts.set0AccelerationStructure))

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
            &m_rtDescriptorSetLayouts.set1Scene))

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
            &m_rtDescriptorSetLayouts.set2Geometry))

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
            &m_rtDescriptorSetLayouts.set3Materials))

        // Set 4: Lighting data
        setLayoutBindings.clear();
        // Light list binding 0
        setLayoutBindings.push_back(
            initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
                    | VK_SHADER_STAGE_MISS_BIT_KHR,
                0,
                1));
        descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
            setLayoutBindings.size());
        VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
            &descriptorLayout,
            nullptr,
            &m_rtDescriptorSetLayouts.set4Lights))

        // Set 5: Offscreen Images
        setLayoutBindings.clear();
        setLayoutBindings.push_back(
            // Binding 0 : Color input
            initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                0));
        setLayoutBindings.push_back(
            // Binding 1 : Normals input
            initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                1));
        setLayoutBindings.push_back(
            // Binding 2 : Reflection and Refraction input
            initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                2));
        setLayoutBindings.push_back(
            // Binding 3 : Depth input
            initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                3));
        descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
            setLayoutBindings.size());
        VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
            &descriptorLayout,
            nullptr,
            &m_rtDescriptorSetLayouts.set5OffscreenImages))

        // Set 6: Storage Images
        setLayoutBindings.clear();
        setLayoutBindings.push_back(
            // Binding 0 : Result
            initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                0));
        descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
            setLayoutBindings.size());
        VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
            &descriptorLayout,
            nullptr,
            &m_rtDescriptorSetLayouts.set6StorageImages))

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
                  m_rtDescriptorSetLayouts.set5OffscreenImages,
                  m_rtDescriptorSetLayouts.set6StorageImages };

        VkPipelineLayoutCreateInfo rayTracingPipelineLayoutCreateInfo
            = initializers::pipelineLayoutCreateInfo(rayTracingSetLayouts.data(),
                rayTracingSetLayouts.size());
        rayTracingPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
        rayTracingPipelineLayoutCreateInfo.pPushConstantRanges = &rtPushConstantRange;
        VKM_CHECK_RESULT(vkCreatePipelineLayout(m_device,
            &rayTracingPipelineLayoutCreateInfo,
            nullptr,
            &m_pipelineLayouts.rayTracing))
    }
    // **** END RAY TRACING LAYOUTS ****
    // **** POSTPROCESS LAYOUTS ****
    {
        // Set 0 Postprocess: Storage Images
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            // Binding 0 : Result image
            initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                0),
        };
        VkDescriptorSetLayoutCreateInfo descriptorLayout
            = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
                setLayoutBindings.size());
        VKM_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
            &descriptorLayout,
            nullptr,
            &m_postProcessDescriptorSetLayouts.set0StorageImages))

        std::array<VkDescriptorSetLayout, 1> setLayouts
            = { m_postProcessDescriptorSetLayouts.set0StorageImages };
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo
            = initializers::pipelineLayoutCreateInfo(setLayouts.data(), setLayouts.size());

        VKM_CHECK_RESULT(vkCreatePipelineLayout(m_device,
            &pipelineLayoutCreateInfo,
            nullptr,
            &m_pipelineLayouts.postProcess))
    }
    // **** END POSTPROCESS LAYOUTS ****
}

void HybridPipelineRT::createDescriptorSets()
{
    // **** RASTERIZATION SETS ****
    {
        // Set 0 Raster: Scene descriptor
        std::vector<VkDescriptorSetLayout> layouts(m_swapChain.imageCount,
            m_rasterDescriptorSetLayouts.set0Scene);
        VkDescriptorSetAllocateInfo allocInfo
            = initializers::descriptorSetAllocateInfo(m_descriptorPool,
                layouts.data(),
                m_swapChain.imageCount);
        m_rasterDescriptorSets.set0Scene.resize(m_swapChain.imageCount);
        VKM_CHECK_RESULT(
            vkAllocateDescriptorSets(m_device, &allocInfo, m_rasterDescriptorSets.set0Scene.data()))
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

        // Set 1 Raster: Material descriptor
        // Materials and Textures descriptor sets
        VkDescriptorSetAllocateInfo textureAllocInfo
            = initializers::descriptorSetAllocateInfo(m_descriptorPool,
                &m_rasterDescriptorSetLayouts.set1Materials,
                1);
        VKM_CHECK_RESULT(vkAllocateDescriptorSets(m_device,
            &textureAllocInfo,
            &m_rasterDescriptorSets.set1Materials))

        std::vector<VkDescriptorImageInfo> textureDescriptors;
        for (auto& texture : m_scene->textures) {
            textureDescriptors.push_back(texture.descriptor);
        }

        VkWriteDescriptorSet writeTextureDescriptorSet
            = initializers::writeDescriptorSet(m_rasterDescriptorSets.set1Materials,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                0,
                textureDescriptors.data(),
                textureDescriptors.size());
        VkWriteDescriptorSet writeMaterialsDescriptorSet
            = initializers::writeDescriptorSet(m_rasterDescriptorSets.set1Materials,
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

        // Set 2 Raster: Lighting descriptor
        VkDescriptorSetAllocateInfo set2AllocInfo
            = initializers::descriptorSetAllocateInfo(m_descriptorPool,
                &m_rasterDescriptorSetLayouts.set2Lights,
                1);
        VKM_CHECK_RESULT(
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
    // **** END: RASTERIZATION SETS ****

    // **** RAY TRACING SETS ****
    {
        // Set 0: Acceleration Structure descriptor
        VkDescriptorSetAllocateInfo set0AllocInfo
            = initializers::descriptorSetAllocateInfo(m_descriptorPool,
                &m_rtDescriptorSetLayouts.set0AccelerationStructure,
                1);
        VKM_CHECK_RESULT(vkAllocateDescriptorSets(m_device,
            &set0AllocInfo,
            &m_rtDescriptorSets.set0AccelerationStructure))

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
            vkAllocateDescriptorSets(m_device, &set1AllocInfo, m_rtDescriptorSets.set1Scene.data()))
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
            vkAllocateDescriptorSets(m_device, &set2AllocInfo, &m_rtDescriptorSets.set2Geometry))

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
            vkAllocateDescriptorSets(m_device, &set3AllocInfo, &m_rtDescriptorSets.set3Materials))

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
            vkAllocateDescriptorSets(m_device, &set4AllocInfo, &m_rtDescriptorSets.set4Lights))
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

        // Set 5: Offscreen images descriptor
        {
            std::vector<VkDescriptorSetLayout> storageImageLayouts(m_swapChain.imageCount,
                m_rtDescriptorSetLayouts.set5OffscreenImages);
            VkDescriptorSetAllocateInfo set5AllocInfo
                = initializers::descriptorSetAllocateInfo(m_descriptorPool,
                    storageImageLayouts.data(),
                    m_swapChain.imageCount);
            m_rtDescriptorSets.set5OffscreenImages.resize(m_swapChain.imageCount);
            VKM_CHECK_RESULT(vkAllocateDescriptorSets(m_device,
                &set5AllocInfo,
                m_rtDescriptorSets.set5OffscreenImages.data()))
        }
        // Set 6: Result image descriptor
        {
            std::vector<VkDescriptorSetLayout> storageImageLayouts(m_swapChain.imageCount,
                m_rtDescriptorSetLayouts.set6StorageImages);
            VkDescriptorSetAllocateInfo set6AllocInfo
                = initializers::descriptorSetAllocateInfo(m_descriptorPool,
                    storageImageLayouts.data(),
                    m_swapChain.imageCount);
            m_rtDescriptorSets.set6StorageImages.resize(m_swapChain.imageCount);
            VKM_CHECK_RESULT(vkAllocateDescriptorSets(m_device,
                &set6AllocInfo,
                m_rtDescriptorSets.set6StorageImages.data()))
        }
    }
    // **** END: RAY TRACING SETS ****

    // **** POSTPROCESS SETS ****
    {
        // Set 0 Storage Images
        std::vector<VkDescriptorSetLayout> storageImageLayouts(m_swapChain.imageCount,
            m_postProcessDescriptorSetLayouts.set0StorageImages);
        VkDescriptorSetAllocateInfo storageImageAllocInfo
            = initializers::descriptorSetAllocateInfo(m_descriptorPool,
                storageImageLayouts.data(),
                m_swapChain.imageCount);
        m_postProcessDescriptorSets.set0StorageImages.resize(m_swapChain.imageCount);
        VKM_CHECK_RESULT(vkAllocateDescriptorSets(m_device,
            &storageImageAllocInfo,
            m_postProcessDescriptorSets.set0StorageImages.data()))
    }
    // **** END: POSTPROCESS SETS ****

    updateResultImageDescriptorSets();
}

void HybridPipelineRT::createStorageImages()
{
    m_offscreenImages.resize(m_swapChain.imageCount);
    for (size_t i = 0; i < m_swapChain.imageCount; i++) {
        m_offscreenImages[i].offscreenColorMultiSample.colorAttachment(VK_FORMAT_R8G8B8A8_UNORM,
            m_width,
            m_height,
            m_vulkanDevice,
            m_queue,
            m_samples,
            VK_IMAGE_USAGE_SAMPLED_BIT);
        m_offscreenImages[i].offscreenColor.colorAttachment(VK_FORMAT_R8G8B8A8_UNORM,
            m_width,
            m_height,
            m_vulkanDevice,
            m_queue,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_SAMPLED_BIT);
        m_offscreenImages[i].offscreenNormalsMultiSample.colorAttachment(VK_FORMAT_R16G16B16A16_SFLOAT,
            m_width,
            m_height,
            m_vulkanDevice,
            m_queue,
            m_samples,
            VK_IMAGE_USAGE_SAMPLED_BIT);
        m_offscreenImages[i].offscreenNormals.colorAttachment(VK_FORMAT_R16G16B16A16_SFLOAT,
            m_width,
            m_height,
            m_vulkanDevice,
            m_queue,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_SAMPLED_BIT);
        m_offscreenImages[i].offscreenReflectRefractMapMultiSample.colorAttachment(
            VK_FORMAT_R16G16B16A16_SFLOAT,
            m_width,
            m_height,
            m_vulkanDevice,
            m_queue,
            m_samples,
            VK_IMAGE_USAGE_SAMPLED_BIT);
        m_offscreenImages[i].offscreenReflectRefractMap.colorAttachment(VK_FORMAT_R16G16B16A16_SFLOAT,
            m_width,
            m_height,
            m_vulkanDevice,
            m_queue,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_SAMPLED_BIT);
        m_offscreenImages[i].offscreenDepthMultiSample.depthAttachment(VK_FORMAT_D32_SFLOAT,
            m_width,
            m_height,
            m_vulkanDevice,
            m_queue,
            m_samples,
            VK_IMAGE_USAGE_SAMPLED_BIT);
        m_offscreenImages[i].offscreenDepth.depthAttachment(VK_FORMAT_D32_SFLOAT,
            m_width,
            m_height,
            m_vulkanDevice,
            m_queue,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_SAMPLED_BIT);
        m_offscreenImages[i].rtResultImage.fromNothing(VK_FORMAT_R8G8B8A8_UNORM,
            m_width,
            m_height,
            m_vulkanDevice,
            m_queue,
            VK_FILTER_NEAREST,
            VK_IMAGE_USAGE_STORAGE_BIT);
    }
}

void HybridPipelineRT::createOffscreenRenderPass()
{
    std::array<VkAttachmentDescription2, 8> attachmentDescriptions = {};
    // Color attachment
    attachmentDescriptions[0].sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    attachmentDescriptions[0].format = VK_FORMAT_R8G8B8A8_UNORM;
    attachmentDescriptions[0].samples = m_samples;
    attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Normals attachment
    attachmentDescriptions[1].sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    attachmentDescriptions[1].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    attachmentDescriptions[1].samples = m_samples;
    attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Reflection and Refraction map attachment
    attachmentDescriptions[2].sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    attachmentDescriptions[2].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    attachmentDescriptions[2].samples = m_samples;
    attachmentDescriptions[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Resolve Color attachment
    attachmentDescriptions[3].sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    attachmentDescriptions[3].format = attachmentDescriptions[0].format;
    attachmentDescriptions[3].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[3].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Resolve multisampling Normals attachment
    attachmentDescriptions[4].sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    attachmentDescriptions[4].format = attachmentDescriptions[1].format;
    attachmentDescriptions[4].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[4].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[4].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[4].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[4].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Resolve multisampling Reflection and Refraction map attachment
    attachmentDescriptions[5].sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    attachmentDescriptions[5].format = attachmentDescriptions[2].format;
    attachmentDescriptions[5].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[5].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[5].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[5].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[5].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[5].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[5].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Depth attachment
    attachmentDescriptions[6].sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    attachmentDescriptions[6].format = VK_FORMAT_D32_SFLOAT;
    attachmentDescriptions[6].samples = m_samples;
    attachmentDescriptions[6].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[6].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[6].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[6].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[6].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[6].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Resolve multisampling (depth) to 1
    attachmentDescriptions[7].sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    attachmentDescriptions[7].format = attachmentDescriptions[6].format;
    attachmentDescriptions[7].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[7].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[7].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[7].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[7].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[7].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[7].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference2 colorReference = {};
    colorReference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference2 normalsReference = {};
    normalsReference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
    normalsReference.attachment = 1;
    normalsReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference2 reflectRefractReference = {};
    reflectRefractReference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
    reflectRefractReference.attachment = 2;
    reflectRefractReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    std::array<VkAttachmentReference2, 3> colorAttachmentReferences
        = { colorReference, normalsReference, reflectRefractReference };

    VkAttachmentReference2 colorResolveReference = {};
    colorResolveReference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
    colorResolveReference.attachment = 3;
    colorResolveReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference2 normalsResolveReference = {};
    normalsResolveReference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
    normalsResolveReference.attachment = 4;
    normalsResolveReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference2 reflectRefractResolveReference = {};
    reflectRefractResolveReference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
    reflectRefractResolveReference.attachment = 5;
    reflectRefractResolveReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    std::array<VkAttachmentReference2, 3> colorAttachmentResolveReferences
        = { colorResolveReference, normalsResolveReference, reflectRefractResolveReference };

    VkAttachmentReference2 depthReference = {};
    depthReference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
    depthReference.attachment = 6;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference2 depthResolveReference = {};
    depthResolveReference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
    depthResolveReference.attachment = 7;
    depthResolveReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkSubpassDescriptionDepthStencilResolve depthResolveReferenceDescription = {};
    depthResolveReferenceDescription.sType
        = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE;
    depthResolveReferenceDescription.pDepthStencilResolveAttachment = &depthResolveReference;
    depthResolveReferenceDescription.depthResolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    depthResolveReferenceDescription.stencilResolveMode = VK_RESOLVE_MODE_NONE;

    VkSubpassDescription2 subpassDescription = {};
    subpassDescription.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount
        = static_cast<uint32_t>(colorAttachmentReferences.size());
    subpassDescription.pColorAttachments = colorAttachmentReferences.data();
    subpassDescription.pDepthStencilAttachment = &depthReference;
    subpassDescription.pResolveAttachments = colorAttachmentResolveReferences.data();
    subpassDescription.pNext = &depthResolveReferenceDescription;

    std::array<VkSubpassDependency2, 2> dependencies = {};

    dependencies[0].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[0].pNext = VK_NULL_HANDLE;

    dependencies[1].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].pNext = VK_NULL_HANDLE;

    // Create the actual renderpass
    VkRenderPassCreateInfo2 renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
    renderPassInfo.pAttachments = attachmentDescriptions.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDescription;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();
    VKM_CHECK_RESULT(
        vkCreateRenderPass2(m_device, &renderPassInfo, nullptr, &m_offscreenRenderPass))
}

void HybridPipelineRT::createOffscreenFramebuffers()
{
    m_offscreenFramebuffers.resize(m_swapChain.imageCount);
    for (uint32_t i = 0; i < m_swapChain.imageCount; i++) {
        std::array<VkImageView, 8> attachments = {};
        attachments[0] = m_offscreenImages[i].offscreenColorMultiSample.getImageView();
        attachments[1] = m_offscreenImages[i].offscreenNormalsMultiSample.getImageView();
        attachments[2] = m_offscreenImages[i].offscreenReflectRefractMapMultiSample.getImageView();
        attachments[3] = m_offscreenImages[i].offscreenColor.getImageView();
        attachments[4] = m_offscreenImages[i].offscreenNormals.getImageView();
        attachments[5] = m_offscreenImages[i].offscreenReflectRefractMap.getImageView();
        attachments[6] = m_offscreenImages[i].offscreenDepthMultiSample.getImageView();
        attachments[7] = m_offscreenImages[i].offscreenDepth.getImageView();

        VkFramebufferCreateInfo framebufferCreateInfo {};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = m_offscreenRenderPass;
        framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferCreateInfo.pAttachments = attachments.data();
        framebufferCreateInfo.width = m_width;
        framebufferCreateInfo.height = m_height;
        framebufferCreateInfo.layers = 1;
        VKM_CHECK_RESULT(vkCreateFramebuffer(m_device,
            &framebufferCreateInfo,
            nullptr,
            &m_offscreenFramebuffers[i]))
    }
}

void HybridPipelineRT::updateResultImageDescriptorSets()
{
    for (uint32_t i = 0; i < m_swapChain.imageCount; i++) {
        // Ray tracing sets
        VkWriteDescriptorSet imageRTInputColorImageWrite
            = initializers::writeDescriptorSet(m_rtDescriptorSets.set5OffscreenImages[i],
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                0,
                &m_offscreenImages[i].offscreenColor.descriptor);
        VkWriteDescriptorSet imageRTInputNormalsImageWrite
            = initializers::writeDescriptorSet(m_rtDescriptorSets.set5OffscreenImages[i],
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                1,
                &m_offscreenImages[i].offscreenNormals.descriptor);
        VkWriteDescriptorSet imageRTInputReflectRefractImageWrite
            = initializers::writeDescriptorSet(m_rtDescriptorSets.set5OffscreenImages[i],
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                2,
                &m_offscreenImages[i].offscreenReflectRefractMap.descriptor);
        VkWriteDescriptorSet imageRTInputDepthWrite
            = initializers::writeDescriptorSet(m_rtDescriptorSets.set5OffscreenImages[i],
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                3,
                &m_offscreenImages[i].offscreenDepth.descriptor);
        std::vector<VkWriteDescriptorSet> writeDescriptorSet5 = { imageRTInputColorImageWrite,
            imageRTInputNormalsImageWrite,
            imageRTInputReflectRefractImageWrite,
            imageRTInputDepthWrite };
        vkUpdateDescriptorSets(m_device,
            static_cast<uint32_t>(writeDescriptorSet5.size()),
            writeDescriptorSet5.data(),
            0,
            VK_NULL_HANDLE);

        VkWriteDescriptorSet imageRTResultWrite
            = initializers::writeDescriptorSet(m_rtDescriptorSets.set6StorageImages[i],
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                0,
                &m_offscreenImages[i].rtResultImage.descriptor);
        std::vector<VkWriteDescriptorSet> writeDescriptorSet6 = { imageRTResultWrite };
        vkUpdateDescriptorSets(m_device,
            static_cast<uint32_t>(writeDescriptorSet6.size()),
            writeDescriptorSet6.data(),
            0,
            VK_NULL_HANDLE);

        // Postprocess sets
        VkWriteDescriptorSet imageResultPostProcessWrite
            = initializers::writeDescriptorSet(m_postProcessDescriptorSets.set0StorageImages[i],
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                0,
                &m_offscreenImages[i].rtResultImage.descriptor);
        std::vector<VkWriteDescriptorSet> writeDescriptorSet0Postprocess
            = { imageResultPostProcessWrite };
        vkUpdateDescriptorSets(m_device,
            static_cast<uint32_t>(writeDescriptorSet0Postprocess.size()),
            writeDescriptorSet0Postprocess.data(),
            0,
            VK_NULL_HANDLE);
    }
}

void HybridPipelineRT::updateUniformBuffers(uint32_t t_currentImage)
{
    memcpy(m_sceneBuffers[t_currentImage].mapped, &m_sceneUniformData, sizeof(m_sceneUniformData));
}

void HybridPipelineRT::onSwapChainRecreation()
{
    // Recreate the result image to fit the new extent size
    for (auto& offscreenImage : m_offscreenImages) {
        offscreenImage.rtResultImage.destroy();
        offscreenImage.offscreenColorMultiSample.destroy();
        offscreenImage.offscreenColor.destroy();
        offscreenImage.offscreenDepthMultiSample.destroy();
        offscreenImage.offscreenDepth.destroy();
        offscreenImage.offscreenNormalsMultiSample.destroy();
        offscreenImage.offscreenNormals.destroy();
        offscreenImage.offscreenReflectRefractMapMultiSample.destroy();
        offscreenImage.offscreenReflectRefractMap.destroy();
    }
    for (auto& frameBuffer : m_offscreenFramebuffers) {
        vkDestroyFramebuffer(m_device, frameBuffer, nullptr);
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
    for (size_t i = 0; i < m_swapChain.imageCount; i++) {
        m_sceneBuffers[i].create(m_vulkanDevice,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            bufferSize);
        VKM_CHECK_RESULT(m_sceneBuffers[i].map())
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
    std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachmentStates = {
        initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
        initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
        initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE)
    };
    VkPipelineColorBlendStateCreateInfo colorBlendState
        = initializers::pipelineColorBlendStateCreateInfo(blendAttachmentStates.size(), blendAttachmentStates.data());
    VkPipelineDepthStencilStateCreateInfo depthStencilState
        = initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE,
            VK_TRUE,
            VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState
        = initializers::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleState
        = initializers::pipelineMultisampleStateCreateInfo(m_samples, 0);
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
    VKM_CHECK_RESULT(vkCreateGraphicsPipelines(m_device,
        m_pipelineCache,
        1,
        &pipelineCreateInfo,
        nullptr,
        &m_pipelines.raster))
}

void HybridPipelineRT::createPostprocessPipeline()
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
    VKM_CHECK_RESULT(vkCreateGraphicsPipelines(m_device,
        m_pipelineCache,
        1,
        &pipelineCreateInfo,
        nullptr,
        &m_pipelines.postProcess))
}

void HybridPipelineRT::assignPushConstants()
{
    m_pathTracerParams = {
        m_ray_tracer_depth, // Max depth
        m_ray_tracer_samples, // samples per frame
    };
}

void HybridPipelineRT::createShaderRTBindingTable()
{
    // Create buffer for the shader binding table
    const uint32_t sbtSize
        = m_rayTracingPipelineProperties.shaderGroupHandleSize * SBT_HP_NUM_SHADER_GROUPS;
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
        SBT_HP_NUM_SHADER_GROUPS,
        sbtSize,
        shaderHandleStorage))
    auto* data = static_cast<uint8_t*>(m_shaderBindingTable.mapped);
    // Copy the shader identifiers to the shader binding table
    data += BaseRTProject::copyRTShaderIdentifier(data, shaderHandleStorage, SBT_HP_RAY_GEN_GROUP);
    data += BaseRTProject::copyRTShaderIdentifier(data, shaderHandleStorage, SBT_HP_MISS_GROUP);
    data += BaseRTProject::copyRTShaderIdentifier(data,
        shaderHandleStorage,
        SBT_HP_SHADOW_MISS_GROUP);
    data += BaseRTProject::copyRTShaderIdentifier(data, shaderHandleStorage, SBT_HP_HIT_GROUP);
    data += BaseRTProject::copyRTShaderIdentifier(data,
        shaderHandleStorage,
        SBT_HP_SHADOW_HIT_GROUP);
    m_shaderBindingTable.unmap();
}

void HybridPipelineRT::createRTPipeline()
{
    std::array<VkPipelineShaderStageCreateInfo, 6> shaderStages {};
    shaderStages[SBT_HP_RAY_GEN_INDEX]
        = loadShader("./shaders/raygen.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    shaderStages[SBT_HP_MISS_INDEX]
        = loadShader("./shaders/miss.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR);
    shaderStages[SBT_HP_SHADOW_MISS_INDEX]
        = loadShader("./shaders/shadow.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR);
    shaderStages[SBT_HP_ANY_HIT_INDEX]
        = loadShader("./shaders/anyhit.rahit.spv", VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
    shaderStages[SBT_HP_CLOSEST_HIT_INDEX]
        = loadShader("./shaders/closesthit.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    shaderStages[SBT_HP_SHADOW_ANY_HIT_INDEX]
        = loadShader("./shaders/shadow.rahit.spv", VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
    /*
      Setup ray tracing shader groups
    */
    std::array<VkRayTracingShaderGroupCreateInfoKHR, SBT_HP_NUM_SHADER_GROUPS> groups {};
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
    groups[SBT_HP_RAY_GEN_GROUP].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[SBT_HP_RAY_GEN_GROUP].generalShader = SBT_HP_RAY_GEN_INDEX;
    // Scene miss shader group
    groups[SBT_HP_MISS_GROUP].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[SBT_HP_MISS_GROUP].generalShader = SBT_HP_MISS_INDEX;
    // Shadow miss shader group
    groups[SBT_HP_SHADOW_MISS_GROUP].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[SBT_HP_SHADOW_MISS_GROUP].generalShader = SBT_HP_SHADOW_MISS_INDEX;
    // Any hit shader group and closest hit shader group
    groups[SBT_HP_HIT_GROUP].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groups[SBT_HP_HIT_GROUP].anyHitShader = SBT_HP_ANY_HIT_INDEX;
    groups[SBT_HP_HIT_GROUP].closestHitShader = SBT_HP_CLOSEST_HIT_INDEX;
    // Shadow closest hit shader group
    groups[SBT_HP_SHADOW_HIT_GROUP].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groups[SBT_HP_SHADOW_HIT_GROUP].anyHitShader = SBT_HP_SHADOW_ANY_HIT_INDEX;

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
        &m_pipelines.rayTracing))
}

void HybridPipelineRT::setSamplesSettings()
{
    VkSampleCountFlags counts = m_deviceProperties.limits.framebufferColorSampleCounts
        & m_deviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_16_BIT) {
        m_samples = VK_SAMPLE_COUNT_16_BIT;
    } else if (counts & VK_SAMPLE_COUNT_8_BIT) {
        m_samples = VK_SAMPLE_COUNT_8_BIT;
    } else {
        m_samples = VK_SAMPLE_COUNT_1_BIT;
    }
}

void HybridPipelineRT::prepare()
{
    BaseRTProject::prepare();
    BaseRTProject::createRTScene("assets/pool/Pool.fbx", m_vertexLayout);
    setSamplesSettings();
    createStorageImages();
    createOffscreenRenderPass();
    createOffscreenFramebuffers();
    createUniformBuffers();
    createDescriptorSetLayout();
    assignPushConstants();
    createRasterPipeline();
    createRTPipeline();
    createShaderRTBindingTable();
    createPostprocessPipeline();
    createDescriptorPool();
    createDescriptorSets();
    buildCommandBuffers();
    m_prepared = true;
}

HybridPipelineRT::~HybridPipelineRT()
{
    vkDestroyRenderPass(m_device, m_offscreenRenderPass, nullptr);
    for (auto& frameBuffer : m_offscreenFramebuffers) {
        vkDestroyFramebuffer(m_device, frameBuffer, nullptr);
    }

    vkDestroyPipeline(m_device, m_pipelines.raster, nullptr);
    vkDestroyPipeline(m_device, m_pipelines.rayTracing, nullptr);
    vkDestroyPipeline(m_device, m_pipelines.postProcess, nullptr);

    vkDestroyPipelineLayout(m_device, m_pipelineLayouts.raster, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayouts.rayTracing, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayouts.postProcess, nullptr);

    vkDestroyDescriptorSetLayout(m_device, m_rasterDescriptorSetLayouts.set0Scene, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rasterDescriptorSetLayouts.set1Materials, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rasterDescriptorSetLayouts.set2Lights, nullptr);
    vkDestroyDescriptorSetLayout(m_device,
        m_rtDescriptorSetLayouts.set0AccelerationStructure,
        nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rtDescriptorSetLayouts.set1Scene, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rtDescriptorSetLayouts.set2Geometry, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rtDescriptorSetLayouts.set3Materials, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rtDescriptorSetLayouts.set4Lights, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rtDescriptorSetLayouts.set5OffscreenImages, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_rtDescriptorSetLayouts.set6StorageImages, nullptr);
    vkDestroyDescriptorSetLayout(m_device,
        m_postProcessDescriptorSetLayouts.set0StorageImages,
        nullptr);

    for (auto& offscreenImage : m_offscreenImages) {
        offscreenImage.rtResultImage.destroy();
        offscreenImage.offscreenColorMultiSample.destroy();
        offscreenImage.offscreenColor.destroy();
        offscreenImage.offscreenDepthMultiSample.destroy();
        offscreenImage.offscreenDepth.destroy();
        offscreenImage.offscreenNormalsMultiSample.destroy();
        offscreenImage.offscreenNormals.destroy();
        offscreenImage.offscreenReflectRefractMapMultiSample.destroy();
        offscreenImage.offscreenReflectRefractMap.destroy();
    }

    m_shaderBindingTable.destroy();
    for (size_t i = 0; i < m_swapChain.imageCount; i++) {
        m_sceneBuffers[i].destroy();
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
    default:
        break;
    }
}

void HybridPipelineRT::viewChanged()
{
    auto camera = m_scene->getCamera();
    m_sceneUniformData.projection = camera->matrices.perspective;
    m_sceneUniformData.view = camera->matrices.view;
    m_sceneUniformData.model = glm::mat4(1.0f);
    m_sceneUniformData.viewInverse = glm::inverse(camera->matrices.view);
    m_sceneUniformData.projInverse = glm::inverse(camera->matrices.perspective);
}
