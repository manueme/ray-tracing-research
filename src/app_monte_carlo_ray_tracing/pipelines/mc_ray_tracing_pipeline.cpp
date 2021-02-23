/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "mc_ray_tracing_pipeline.h"
#include "../constants.h"
#include <array>
#include <vector>

MCRayTracingPipeline::MCRayTracingPipeline(
    Device* t_vulkanDevice, uint32_t t_maxDepth, uint32_t t_sampleCount)
    : RayTracingBasePipeline(t_vulkanDevice, t_maxDepth, t_sampleCount)
{
}

MCRayTracingPipeline::~MCRayTracingPipeline()
{
    vkDestroyDescriptorSetLayout(m_device,
        m_descriptorSetLayouts.set0AccelerationStructure,
        nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set1Scene, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set2Geometry, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set3Materials, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set4Lights, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set5ResultImage, nullptr);
};

void MCRayTracingPipeline::buildCommandBuffer(
    VkCommandBuffer t_commandBuffer, uint32_t t_width, uint32_t t_height)
{
    /*
    Dispatch the ray tracing commands
    */
    vkCmdBindPipeline(t_commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline);
    std::vector<VkDescriptorSet> rtDescriptorSets = { m_descriptorSets.set0AccelerationStructure,
        m_descriptorSets.set1Scene,
        m_descriptorSets.set2Geometry,
        m_descriptorSets.set3Materials,
        m_descriptorSets.set4Lights,
        m_descriptorSets.set5ResultImage };
    vkCmdBindDescriptorSets(t_commandBuffer,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        m_pipelineLayout,
        0,
        rtDescriptorSets.size(),
        rtDescriptorSets.data(),
        0,
        nullptr);
    vkCmdPushConstants(t_commandBuffer,
        m_pipelineLayout,
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

    vkCmdTraceRaysKHR(t_commandBuffer,
        &rayGenSbtRegion,
        &missSbtRegion,
        &hitSbtRegion,
        &emptySbtEntry,
        t_width,
        t_height,
        1);
}

void MCRayTracingPipeline::createDescriptorSetsLayout(Scene* t_scene)
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
    CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_descriptorSetLayouts.set0AccelerationStructure));

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
    CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_descriptorSetLayouts.set1Scene));

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
    CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_descriptorSetLayouts.set2Geometry));

    // Set 3: Textures data
    setLayoutBindings.clear();
    // Texture list binding 0
    setLayoutBindings.push_back(
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            0,
            t_scene->textures.size()));
    // Material list binding 1
    setLayoutBindings.push_back(
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            1));
    descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
        setLayoutBindings.size());
    CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_descriptorSetLayouts.set3Materials));

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
    CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_descriptorSetLayouts.set4Lights));

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
        // Binding 2 : Result Image Normal Map
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            2));
    setLayoutBindings.push_back(
        // Binding  3: Result Image Albedo
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            3));

    descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
        setLayoutBindings.size());
    CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_descriptorSetLayouts.set5ResultImage));

    // Ray Tracing Pipeline Layout
    // Push constant to pass path tracer parameters
    VkPushConstantRange rtPushConstantRange
        = initializers::pushConstantRange(VK_SHADER_STAGE_RAYGEN_BIT_KHR
                | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
            sizeof(PathTracerParameters),
            0);
    std::array<VkDescriptorSetLayout, 6> rayTracingSetLayouts
        = { m_descriptorSetLayouts.set0AccelerationStructure,
              m_descriptorSetLayouts.set1Scene,
              m_descriptorSetLayouts.set2Geometry,
              m_descriptorSetLayouts.set3Materials,
              m_descriptorSetLayouts.set4Lights,
              m_descriptorSetLayouts.set5ResultImage };

    VkPipelineLayoutCreateInfo rayTracingPipelineLayoutCreateInfo
        = initializers::pipelineLayoutCreateInfo(rayTracingSetLayouts.data(),
            rayTracingSetLayouts.size());
    rayTracingPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    rayTracingPipelineLayoutCreateInfo.pPushConstantRanges = &rtPushConstantRange;
    CHECK_RESULT(vkCreatePipelineLayout(m_device,
        &rayTracingPipelineLayoutCreateInfo,
        nullptr,
        &m_pipelineLayout));
}

void MCRayTracingPipeline::createDescriptorSets(VkDescriptorPool t_descriptorPool, Scene* t_scene,
    Buffer* t_sceneBuffer, Buffer* t_instancesBuffer, Buffer* t_lightsBuffer,
    Buffer* t_materialsBuffer)
{
    // Set 0: Acceleration Structure descriptor
    VkDescriptorSetAllocateInfo set0AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            &m_descriptorSetLayouts.set0AccelerationStructure,
            1);
    CHECK_RESULT(vkAllocateDescriptorSets(m_device,
        &set0AllocInfo,
        &m_descriptorSets.set0AccelerationStructure));

    VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo {};
    descriptorAccelerationStructureInfo.sType
        = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    auto topLevelHandle = m_topLevelAS.getHandle();
    descriptorAccelerationStructureInfo.pAccelerationStructures = &topLevelHandle;

    VkWriteDescriptorSet accelerationStructureWrite {};
    accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    // The specialized acceleration structure descriptor has to be chained
    accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
    accelerationStructureWrite.dstSet = m_descriptorSets.set0AccelerationStructure;
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
    VkDescriptorSetAllocateInfo set1AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            &m_descriptorSetLayouts.set1Scene,
            1);
    CHECK_RESULT(vkAllocateDescriptorSets(m_device, &set1AllocInfo, &m_descriptorSets.set1Scene));
    VkWriteDescriptorSet uniformBufferWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set1Scene,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            0,
            &t_sceneBuffer->descriptor);
    std::vector<VkWriteDescriptorSet> writeDescriptorSet1 = { uniformBufferWrite };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet1.size()),
        writeDescriptorSet1.data(),
        0,
        VK_NULL_HANDLE);

    // Set 2: Geometry descriptor
    VkDescriptorSetAllocateInfo set2AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            &m_descriptorSetLayouts.set2Geometry,
            1);
    CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &set2AllocInfo, &m_descriptorSets.set2Geometry));

    VkDescriptorBufferInfo vertexBufferDescriptor {};
    vertexBufferDescriptor.buffer = t_scene->vertices.buffer;
    vertexBufferDescriptor.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo indexBufferDescriptor {};
    indexBufferDescriptor.buffer = t_scene->indices.buffer;
    indexBufferDescriptor.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet vertexBufferWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set2Geometry,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            0,
            &vertexBufferDescriptor);
    VkWriteDescriptorSet indexBufferWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set2Geometry,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            1,
            &indexBufferDescriptor);
    VkWriteDescriptorSet materialIndexBufferWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set2Geometry,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            2,
            &t_instancesBuffer->descriptor);
    std::vector<VkWriteDescriptorSet> writeDescriptorSet2
        = { vertexBufferWrite, indexBufferWrite, materialIndexBufferWrite };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet2.size()),
        writeDescriptorSet2.data(),
        0,
        VK_NULL_HANDLE);

    // Set 3: Materials and Textures descriptor
    VkDescriptorSetAllocateInfo set3AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            &m_descriptorSetLayouts.set3Materials,
            1);
    CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &set3AllocInfo, &m_descriptorSets.set3Materials));

    std::vector<VkWriteDescriptorSet> writeDescriptorSet3 = {};
    std::vector<VkDescriptorImageInfo> textureDescriptors;
    VkWriteDescriptorSet writeTextureDescriptorSet;
    if (!t_scene->textures.empty()) {
        for (auto& texture : t_scene->textures) {
            textureDescriptors.push_back(texture.descriptor);
        }
        writeTextureDescriptorSet = initializers::writeDescriptorSet(m_descriptorSets.set3Materials,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            0,
            textureDescriptors.data(),
            textureDescriptors.size());
        writeDescriptorSet3.push_back(writeTextureDescriptorSet);
    }
    VkWriteDescriptorSet writeMaterialsDescriptorSet
        = initializers::writeDescriptorSet(m_descriptorSets.set3Materials,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            1,
            &t_materialsBuffer->descriptor);
    writeDescriptorSet3.push_back(writeMaterialsDescriptorSet);

    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet3.size()),
        writeDescriptorSet3.data(),
        0,
        nullptr);

    // Set 4: Lighting descriptor
    VkDescriptorSetAllocateInfo set4AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            &m_descriptorSetLayouts.set4Lights,
            1);
    CHECK_RESULT(vkAllocateDescriptorSets(m_device, &set4AllocInfo, &m_descriptorSets.set4Lights));
    VkWriteDescriptorSet writeLightsDescriptorSet
        = initializers::writeDescriptorSet(m_descriptorSets.set4Lights,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            0,
            &t_lightsBuffer->descriptor);

    std::vector<VkWriteDescriptorSet> writeDescriptorSet4 = { writeLightsDescriptorSet };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet4.size()),
        writeDescriptorSet4.data(),
        0,
        nullptr);

    // Set 5: Result image descriptor
    VkDescriptorSetAllocateInfo set5AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            &m_descriptorSetLayouts.set5ResultImage,
            1);
    CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &set5AllocInfo, &m_descriptorSets.set5ResultImage));
}

void MCRayTracingPipeline::updateResultImageDescriptorSets(
    Texture* t_result, Texture* t_depthMap, Texture* t_normalMap, Texture* t_albedo)
{
    VkWriteDescriptorSet resultImageWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set5ResultImage,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            0,
            &t_result->descriptor);
    VkWriteDescriptorSet resultDepthMapWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set5ResultImage,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            1,
            &t_depthMap->descriptor);
    VkWriteDescriptorSet resultNormalMapWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set5ResultImage,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            2,
            &t_normalMap->descriptor);
    VkWriteDescriptorSet resultAlbedoWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set5ResultImage,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            3,
            &t_albedo->descriptor);
    std::vector<VkWriteDescriptorSet> writeDescriptorSet5
        = { resultImageWrite, resultDepthMapWrite, resultNormalMapWrite, resultAlbedoWrite };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet5.size()),
        writeDescriptorSet5.data(),
        0,
        VK_NULL_HANDLE);
}

void MCRayTracingPipeline::createShaderBindingTable()
{
    // Create buffer for the shader binding table
    const uint32_t sbtSize
        = m_rayTracingPipelineProperties.shaderGroupHandleSize * SBT_NUM_SHADER_GROUPS;
    m_shaderBindingTable.create(m_vulkanDevice,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        sbtSize);
    m_shaderBindingTable.map();
    auto shaderHandleStorage = new uint8_t[sbtSize];
    // Get shader identifiers
    CHECK_RESULT(vkGetRayTracingShaderGroupHandlesKHR(m_device,
        m_pipeline,
        0,
        SBT_NUM_SHADER_GROUPS,
        sbtSize,
        shaderHandleStorage));
    auto* data = static_cast<uint8_t*>(m_shaderBindingTable.mapped);
    // Copy the shader identifiers to the shader binding table
    data += copyRTShaderIdentifier(data, shaderHandleStorage, SBT_RAY_GEN_GROUP);
    data += copyRTShaderIdentifier(data, shaderHandleStorage, SBT_MISS_GROUP);
    data += copyRTShaderIdentifier(data, shaderHandleStorage, SBT_SHADOW_MISS_GROUP);
    data += copyRTShaderIdentifier(data, shaderHandleStorage, SBT_HIT_GROUP);
    data += copyRTShaderIdentifier(data, shaderHandleStorage, SBT_SHADOW_HIT_GROUP);
    m_shaderBindingTable.unmap();
}
