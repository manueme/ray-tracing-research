/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "hy_ray_tracing_pipeline.h"
#include "../app_hybrid_ray_tracing/constants.h"
#include "core/device.h"
#include "scene/scene.h"
#include "tools/tools.h"
#include <array>
#include <thread>
#include <vector>

HyRayTracingPipeline::HyRayTracingPipeline(Device* t_vulkanDevice, uint32_t t_maxDepth, uint32_t t_sampleCount)
    : m_device(t_vulkanDevice->logicalDevice)
    , m_vulkanDevice(t_vulkanDevice)
{
    m_vulkanDevice = t_vulkanDevice;
    m_pathTracerParams = {
        t_maxDepth, // Max depth
        t_sampleCount, // samples per frame
    };
    initFunctionPointers();
    getDeviceRayTracingProperties();
}

void HyRayTracingPipeline::getDeviceRayTracingProperties()
{
    m_rayTracingPipelineProperties = {};
    m_rayTracingPipelineProperties.sType
        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 deviceProps2 {};
    deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProps2.pNext = &m_rayTracingPipelineProperties;
    vkGetPhysicalDeviceProperties2(m_vulkanDevice->physicalDevice, &deviceProps2);
}

HyRayTracingPipeline::~HyRayTracingPipeline()
{
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device,
        m_descriptorSetLayouts.set0AccelerationStructure,
        nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set1Scene, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set2Geometry, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set3Materials, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set4Lights, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set5OffscreenImages, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayouts.set6StorageImages, nullptr);
    m_shaderBindingTable.destroy();

    m_topLevelAS.destroy();
    for (auto& blas : m_bottomLevelAS) {
        blas.destroy();
    }
};

void HyRayTracingPipeline::buildCommandBuffer(
    uint32_t t_index, VkCommandBuffer t_commandBuffer, uint32_t t_width, uint32_t t_height)
{
    vkCmdBindPipeline(t_commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline);
    std::vector<VkDescriptorSet> rtDescriptorSets = { m_descriptorSets.set0AccelerationStructure,
        m_descriptorSets.set1Scene[t_index],
        m_descriptorSets.set2Geometry,
        m_descriptorSets.set3Materials,
        m_descriptorSets.set4Lights,
        m_descriptorSets.set5OffscreenImages[t_index],
        m_descriptorSets.set6StorageImages[t_index] };
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

void HyRayTracingPipeline::createDescriptorSetsLayout(Scene* t_scene)
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
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
                | VK_SHADER_STAGE_MISS_BIT_KHR,
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
    CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_descriptorSetLayouts.set5OffscreenImages));

    // Set 6: Storage Images
    setLayoutBindings.clear();
    setLayoutBindings.push_back(
        // Binding 0 : Result
        initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            0));
    descriptorLayout = initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(),
        setLayoutBindings.size());
    CHECK_RESULT(vkCreateDescriptorSetLayout(m_device,
        &descriptorLayout,
        nullptr,
        &m_descriptorSetLayouts.set6StorageImages))

    // Ray Tracing Pipeline Layout
    // Push constant to pass path tracer parameters
    VkPushConstantRange rtPushConstantRange
        = initializers::pushConstantRange(VK_SHADER_STAGE_RAYGEN_BIT_KHR
                | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
            sizeof(PathTracerParameters),
            0);
    std::array<VkDescriptorSetLayout, 7> rayTracingSetLayouts
        = { m_descriptorSetLayouts.set0AccelerationStructure,
              m_descriptorSetLayouts.set1Scene,
              m_descriptorSetLayouts.set2Geometry,
              m_descriptorSetLayouts.set3Materials,
              m_descriptorSetLayouts.set4Lights,
              m_descriptorSetLayouts.set5OffscreenImages,
              m_descriptorSetLayouts.set6StorageImages };

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

void HyRayTracingPipeline::createPipeline(std::vector<VkPipelineShaderStageCreateInfo> t_shaderStages,
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> t_shaderGroups)
{
    VkRayTracingPipelineCreateInfoKHR rayPipelineInfo {};
    rayPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    rayPipelineInfo.stageCount = static_cast<uint32_t>(t_shaderStages.size());
    rayPipelineInfo.pStages = t_shaderStages.data();
    rayPipelineInfo.groupCount = static_cast<uint32_t>(t_shaderGroups.size());
    rayPipelineInfo.pGroups = t_shaderGroups.data();
    rayPipelineInfo.maxPipelineRayRecursionDepth = m_pathTracerParams.maxDepth;
    rayPipelineInfo.layout = m_pipelineLayout;
    CHECK_RESULT(vkCreateRayTracingPipelinesKHR(m_device,
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        1,
        &rayPipelineInfo,
        nullptr,
        &m_pipeline));

    createShaderBindingTable();
}

void HyRayTracingPipeline::createDescriptorSets(VkDescriptorPool t_descriptorPool, Scene* t_scene,
    uint32_t t_swapChainCount, std::vector<Buffer>& t_sceneBuffers, Buffer* t_instancesBuffer,
    Buffer* t_lightsBuffer, Buffer* t_materialsBuffer)
{
    // Set 0: Acceleration Structure descriptor
    VkDescriptorSetAllocateInfo set0AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            &m_descriptorSetLayouts.set0AccelerationStructure,
            1);
    CHECK_RESULT(vkAllocateDescriptorSets(m_device,
        &set0AllocInfo,
        &m_descriptorSets.set0AccelerationStructure))

    VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo {};
    descriptorAccelerationStructureInfo.sType
        = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    auto topLevelHandle = m_topLevelAS.getHandle();
    descriptorAccelerationStructureInfo.pAccelerationStructures = &topLevelHandle;

    VkWriteDescriptorSet accelerationStructureWrite {};
    accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
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
    std::vector<VkDescriptorSetLayout> set1Layouts(t_swapChainCount,
        m_descriptorSetLayouts.set1Scene);
    VkDescriptorSetAllocateInfo set1AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            set1Layouts.data(),
            t_swapChainCount);
    m_descriptorSets.set1Scene.resize(t_swapChainCount);
    CHECK_RESULT(
        vkAllocateDescriptorSets(m_device, &set1AllocInfo, m_descriptorSets.set1Scene.data()))
    for (size_t i = 0; i < t_swapChainCount; i++) {
        VkWriteDescriptorSet uniformBufferWrite
            = initializers::writeDescriptorSet(m_descriptorSets.set1Scene[i],
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                0,
                &t_sceneBuffers[i].descriptor);
        std::vector<VkWriteDescriptorSet> writeDescriptorSet1 = { uniformBufferWrite };
        vkUpdateDescriptorSets(m_device,
            static_cast<uint32_t>(writeDescriptorSet1.size()),
            writeDescriptorSet1.data(),
            0,
            VK_NULL_HANDLE);
    }

    // Set 2: Geometry descriptor
    VkDescriptorSetAllocateInfo set2AllocInfo
        = initializers::descriptorSetAllocateInfo(t_descriptorPool,
            &m_descriptorSetLayouts.set2Geometry,
            1);
    CHECK_RESULT(vkAllocateDescriptorSets(m_device, &set2AllocInfo, &m_descriptorSets.set2Geometry))

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
        vkAllocateDescriptorSets(m_device, &set3AllocInfo, &m_descriptorSets.set3Materials))

    std::vector<VkWriteDescriptorSet> writeDescriptorSet3 = {};
    VkWriteDescriptorSet writeTextureDescriptorSet;
    std::vector<VkDescriptorImageInfo> textureDescriptors = {};
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
    CHECK_RESULT(vkAllocateDescriptorSets(m_device, &set4AllocInfo, &m_descriptorSets.set4Lights))
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

    // Set 5: Offscreen images descriptor
    {
        std::vector<VkDescriptorSetLayout> storageImageLayouts(t_swapChainCount,
            m_descriptorSetLayouts.set5OffscreenImages);
        VkDescriptorSetAllocateInfo set5AllocInfo
            = initializers::descriptorSetAllocateInfo(t_descriptorPool,
                storageImageLayouts.data(),
                t_swapChainCount);
        m_descriptorSets.set5OffscreenImages.resize(t_swapChainCount);
        CHECK_RESULT(vkAllocateDescriptorSets(m_device,
            &set5AllocInfo,
            m_descriptorSets.set5OffscreenImages.data()))
    }
    // Set 6: Result image descriptor
    {
        std::vector<VkDescriptorSetLayout> storageImageLayouts(t_swapChainCount,
            m_descriptorSetLayouts.set6StorageImages);
        VkDescriptorSetAllocateInfo set6AllocInfo
            = initializers::descriptorSetAllocateInfo(t_descriptorPool,
                storageImageLayouts.data(),
                t_swapChainCount);
        m_descriptorSets.set6StorageImages.resize(t_swapChainCount);
        CHECK_RESULT(vkAllocateDescriptorSets(m_device,
            &set6AllocInfo,
            m_descriptorSets.set6StorageImages.data()))
    }
}

void HyRayTracingPipeline::updateResultImageDescriptorSets(uint32_t t_index,
    Texture* t_offscreenColor, Texture* t_offscreenNormals, Texture* t_offscreenReflectRefractMap,
    Texture* t_offscreenDepth, Texture* t_result)
{
    // Ray tracing sets
    VkWriteDescriptorSet imageRTInputColorImageWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set5OffscreenImages[t_index],
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            0,
            &t_offscreenColor->descriptor);
    VkWriteDescriptorSet imageRTInputNormalsImageWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set5OffscreenImages[t_index],
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            &t_offscreenNormals->descriptor);
    VkWriteDescriptorSet imageRTInputReflectRefractImageWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set5OffscreenImages[t_index],
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            2,
            &t_offscreenReflectRefractMap->descriptor);
    VkWriteDescriptorSet imageRTInputDepthWrite
        = initializers::writeDescriptorSet(m_descriptorSets.set5OffscreenImages[t_index],
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            3,
            &t_offscreenDepth->descriptor);
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
        = initializers::writeDescriptorSet(m_descriptorSets.set6StorageImages[t_index],
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            0,
            &t_result->descriptor);
    std::vector<VkWriteDescriptorSet> writeDescriptorSet6 = { imageRTResultWrite };
    vkUpdateDescriptorSets(m_device,
        static_cast<uint32_t>(writeDescriptorSet6.size()),
        writeDescriptorSet6.data(),
        0,
        VK_NULL_HANDLE);
}

void HyRayTracingPipeline::createShaderBindingTable()
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

VkDeviceSize HyRayTracingPipeline::copyRTShaderIdentifier(
    uint8_t* t_data, const uint8_t* t_shaderHandleStorage, uint32_t t_groupIndex) const
{
    const uint32_t shaderGroupHandleSize = m_rayTracingPipelineProperties.shaderGroupHandleSize;
    memcpy(t_data,
        t_shaderHandleStorage + t_groupIndex * shaderGroupHandleSize,
        shaderGroupHandleSize);
    return shaderGroupHandleSize;
}

Scene* HyRayTracingPipeline::createRTScene(
    VkQueue t_queue, const std::string& t_modelPath, SceneVertexLayout t_vertexLayout)
{
    // Models
    SceneCreateInfo modelCreateInfo(glm::vec3(1.0f), glm::vec3(1.0f), glm::vec3(0.0f));
    modelCreateInfo.memoryPropertyFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    auto scene = new Scene();
    std::thread loadSceneThread(&Scene::loadFromFile,
        scene,
        t_modelPath,
        t_vertexLayout,
        &modelCreateInfo,
        m_vulkanDevice,
        t_queue);
    loadSceneThread.detach();
    while (!scene->isLoaded()) {
        glfwWaitEventsTimeout(1);
    }
    std::cout << "\nGenerating acceleration structure..." << std::endl;

    // One Geometry per blas for this scene
    VkAccelerationStructureGeometryKHR geometry {};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.geometry.triangles.sType
        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry.geometry.triangles.vertexData.deviceAddress = scene->vertices.getDeviceAddress();
    geometry.geometry.triangles.vertexStride = t_vertexLayout.stride();
    geometry.geometry.triangles.maxVertex = scene->vertexCount;
    geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geometry.geometry.triangles.indexData.deviceAddress = scene->indices.getDeviceAddress();
    geometry.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;

    std::vector<BlasCreateInfo> blases;
    for (auto i = 0; i < scene->meshes.size(); i++) {
        const auto mesh = scene->meshes[i];
        VkAccelerationStructureBuildRangeInfoKHR meshOffsetInfo {};
        meshOffsetInfo.primitiveCount = mesh.getIndexCount() / 3;
        meshOffsetInfo.primitiveOffset = mesh.getIndexOffset();
        meshOffsetInfo.firstVertex = mesh.getVertexBase();

        BlasCreateInfo blas; // One blas per mesh. All the meshes are part of the same geometry with
        // different offsets
        blas.geomery = { geometry };
        blas.meshes = { meshOffsetInfo };
        blases.emplace_back(blas);

        scene->createMeshInstance(i, i);
    }
    createBottomLevelAccelerationStructure(t_queue, blases);

    VkTransformMatrixKHR transform = {
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
    };
    TlasCreateInfo geometryInstances;
    for (auto meshInstance : scene->instances) {
        VkAccelerationStructureInstanceKHR instance = {};
        instance.transform = transform;
        instance.instanceCustomIndex = meshInstance.getBlasIdx();
        instance.mask = AS_FLAG_EVERYTHING;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference
            = m_bottomLevelAS[meshInstance.getBlasIdx()].getDeviceAddress();
        geometryInstances.instances.push_back(instance);
        geometryInstances.update = false;
    }
    createTopLevelAccelerationStructure(t_queue, geometryInstances);
    debug::printPercentage(0, 1);

    return scene;
}

void HyRayTracingPipeline::createBottomLevelAccelerationStructure(
    VkQueue t_queue, const std::vector<BlasCreateInfo>& t_blases)

{
    m_bottomLevelAS.resize(t_blases.size());
    const auto blasCount = static_cast<uint32_t>(t_blases.size());
    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> buildInfos(blasCount);
    for (uint32_t idx = 0; idx < blasCount; idx++) {
        buildInfos[idx].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfos[idx].flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildInfos[idx].geometryCount = t_blases[idx].geomery.size();
        buildInfos[idx].pGeometries = t_blases[idx].geomery.data();
        buildInfos[idx].mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfos[idx].type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildInfos[idx].srcAccelerationStructure = VK_NULL_HANDLE;
    }

    VkDeviceSize maxScratch { 0 };
    for (uint32_t idx = 0; idx < blasCount; idx++) {
        const auto meshCount = static_cast<uint32_t>(t_blases[idx].meshes.size());
        std::vector<uint32_t> maxPrimCount(meshCount);
        for (auto tt = 0; tt < meshCount; tt++) {
            maxPrimCount[tt] = t_blases[idx].meshes[tt].primitiveCount;
        }
        VkAccelerationStructureBuildSizesInfoKHR sizeInfo {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
        };
        vkGetAccelerationStructureBuildSizesKHR(m_device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfos[idx],
            maxPrimCount.data(),
            &sizeInfo);
        // Create acceleration structure object. Not yet bound to memory.
        m_bottomLevelAS[idx] = AccelerationStructure(m_vulkanDevice,
            VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            sizeInfo);
        buildInfos[idx].dstAccelerationStructure = m_bottomLevelAS[idx].getHandle();
        maxScratch = glm::max(maxScratch, sizeInfo.buildScratchSize);
    }

    Buffer scratchBuffer;
    scratchBuffer.create(m_vulkanDevice,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        maxScratch);
    VkCommandBuffer cmdBuffer
        = m_vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    for (uint32_t idx = 0; idx < blasCount; idx++) {
        const auto meshCount = t_blases[idx].meshes.size();
        std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> pBuildOffset(meshCount);
        for (size_t infoIdx = 0; infoIdx < meshCount; infoIdx++) {
            pBuildOffset[infoIdx] = &t_blases[idx].meshes[infoIdx];
        }
        buildInfos[idx].scratchData.deviceAddress = scratchBuffer.getDeviceAddress();
        vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfos[idx], pBuildOffset.data());

        // Since the scratch buffer is reused across builds, we need a barrier to ensure one build
        // is finished before starting the next one
        VkMemoryBarrier barrier { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            0,
            1,
            &barrier,
            0,
            nullptr,
            0,
            nullptr);
    }
    m_vulkanDevice->flushCommandBuffer(cmdBuffer, t_queue);
    scratchBuffer.destroy();
}

void HyRayTracingPipeline::createTopLevelAccelerationStructure(
    VkQueue t_queue, TlasCreateInfo t_tlasCreateInfo)
{
    // Buffer for instance data
    Buffer instancesBuffer;
    instancesBuffer.create(m_vulkanDevice,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        sizeof(VkAccelerationStructureInstanceKHR) * t_tlasCreateInfo.instances.size(),
        t_tlasCreateInfo.instances.data());

    VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress {};
    instanceDataDeviceAddress.deviceAddress = instancesBuffer.getDeviceAddress();
    VkAccelerationStructureGeometryKHR accelerationStructureGeometry {};
    accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    accelerationStructureGeometry.geometry.instances.sType
        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
    accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

    // Find sizes
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR
    };
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &accelerationStructureGeometry;
    buildInfo.mode = t_tlasCreateInfo.update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
                                             : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

    auto instanceCount = static_cast<uint32_t>(t_tlasCreateInfo.instances.size());
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    vkGetAccelerationStructureBuildSizesKHR(m_device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &instanceCount,
        &sizeInfo);

    // Create TLAS
    if (t_tlasCreateInfo.update == false) {
        m_topLevelAS = AccelerationStructure(m_vulkanDevice,
            VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            sizeInfo);
    } else {
        buildInfo.srcAccelerationStructure = m_topLevelAS.getHandle();
    }
    buildInfo.dstAccelerationStructure = m_topLevelAS.getHandle();

    Buffer scratchBuffer;
    scratchBuffer.create(m_vulkanDevice,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        sizeInfo.buildScratchSize);
    buildInfo.scratchData.deviceAddress = scratchBuffer.getDeviceAddress();

    VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo {};
    accelerationStructureBuildRangeInfo.primitiveCount = instanceCount;
    accelerationStructureBuildRangeInfo.primitiveOffset = 0;
    accelerationStructureBuildRangeInfo.firstVertex = 0;
    accelerationStructureBuildRangeInfo.transformOffset = 0;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos
        = { &accelerationStructureBuildRangeInfo };

    // Acceleration structure needs to be build on the device
    VkCommandBuffer cmdBuffer
        = m_vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    vkCmdBuildAccelerationStructuresKHR(cmdBuffer,
        1,
        &buildInfo,
        accelerationBuildStructureRangeInfos.data());

    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR
        | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR
        | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    vkCmdPipelineBarrier(cmdBuffer,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        0,
        1,
        &memoryBarrier,
        0,
        0,
        0,
        0);

    m_vulkanDevice->flushCommandBuffer(cmdBuffer, t_queue);
    scratchBuffer.destroy();
    instancesBuffer.destroy();
}
