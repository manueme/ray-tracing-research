/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "ray_tracing_base_pipeline.h"
#include "core/device.h"
#include "scene/scene.h"
#include "shaders/shared_constants.h"
#include "tools/tools.h"
#include <thread>
#include <vector>

RayTracingBasePipeline::RayTracingBasePipeline(Device* t_vulkanDevice, uint32_t t_maxDepth,
    uint32_t t_sampleCount)
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

void RayTracingBasePipeline::getDeviceRayTracingProperties()
{
    m_rayTracingPipelineProperties = {};
    m_rayTracingPipelineProperties.sType
        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 deviceProps2 {};
    deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProps2.pNext = &m_rayTracingPipelineProperties;
    vkGetPhysicalDeviceProperties2(m_vulkanDevice->physicalDevice, &deviceProps2);
}

RayTracingBasePipeline::~RayTracingBasePipeline()
{
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    m_shaderBindingTable.destroy();
    m_topLevelAS.destroy();
    for (auto& blas : m_bottomLevelAS) {
        blas.destroy();
    }
};

void RayTracingBasePipeline::createPipeline(
    std::vector<VkPipelineShaderStageCreateInfo> t_shaderStages,
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

VkDeviceSize RayTracingBasePipeline::copyRTShaderIdentifier(uint8_t* t_data,
    const uint8_t* t_shaderHandleStorage, uint32_t t_groupIndex) const
{
    const uint32_t shaderGroupHandleSize = m_rayTracingPipelineProperties.shaderGroupHandleSize;
    memcpy(t_data,
        t_shaderHandleStorage + t_groupIndex * shaderGroupHandleSize,
        shaderGroupHandleSize);
    return shaderGroupHandleSize;
}

Scene* RayTracingBasePipeline::createRTScene(VkQueue t_queue, const std::string& t_modelPath,
    SceneVertexLayout t_vertexLayout)
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
    for (auto i = 0; i < scene->meshes.size(); ++i) {
        const auto mesh = scene->meshes[i];
        uint32_t primitiveCount = mesh.getIndexCount() / 3;

        // Skip meshes with less than 1 triangle (3 indices)
        if (primitiveCount == 0) {
            continue;
        }

        VkAccelerationStructureBuildRangeInfoKHR meshOffsetInfo {};
        meshOffsetInfo.primitiveCount = primitiveCount;
        meshOffsetInfo.primitiveOffset = mesh.getIndexOffset();
        meshOffsetInfo.firstVertex = mesh.getVertexBase();

        BlasCreateInfo blas; // One blas per mesh. All the meshes are part of the same geometry with
        // different offsets
        blas.geomery = { geometry };
        blas.meshes = { meshOffsetInfo };
        blases.emplace_back(blas);

        uint32_t blasIdx = static_cast<uint32_t>(blases.size() - 1);
        scene->createMeshInstance(blasIdx, i);
    }

    if (blases.empty()) {
        throw std::runtime_error("No valid meshes found for acceleration structure");
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

void RayTracingBasePipeline::createBottomLevelAccelerationStructure(VkQueue t_queue,
    const std::vector<BlasCreateInfo>& t_blases)

{
    m_bottomLevelAS.resize(t_blases.size());
    const auto blasCount = static_cast<uint32_t>(t_blases.size());
    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> buildInfos(blasCount);
    for (uint32_t idx = 0; idx < blasCount; ++idx) {
        buildInfos[idx].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfos[idx].flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildInfos[idx].geometryCount = t_blases[idx].geomery.size();
        buildInfos[idx].pGeometries = t_blases[idx].geomery.data();
        buildInfos[idx].mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfos[idx].type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildInfos[idx].srcAccelerationStructure = VK_NULL_HANDLE;
    }

    VkDeviceSize maxScratch { 0 };
    for (uint32_t idx = 0; idx < blasCount; ++idx) {
        const auto meshCount = static_cast<uint32_t>(t_blases[idx].meshes.size());
        std::vector<uint32_t> maxPrimCount(meshCount);
        for (auto tt = 0; tt < meshCount; ++tt) {
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

        if (sizeInfo.accelerationStructureSize == 0) {
            throw std::runtime_error("Cannot create BLAS with zero size");
        }

        // Create acceleration structure object. Not yet bound to memory.
        m_bottomLevelAS[idx] = AccelerationStructure(m_vulkanDevice,
            VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            sizeInfo);
        buildInfos[idx].dstAccelerationStructure = m_bottomLevelAS[idx].getHandle();
        maxScratch = glm::max(maxScratch, sizeInfo.buildScratchSize);
    }

    if (maxScratch == 0) {
        throw std::runtime_error("Cannot create acceleration structure with zero scratch size");
    }

    Buffer scratchBuffer;
    scratchBuffer.create(m_vulkanDevice,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        maxScratch);
    VkCommandBuffer cmdBuffer
        = m_vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    for (uint32_t idx = 0; idx < blasCount; ++idx) {
        const auto meshCount = t_blases[idx].meshes.size();
        std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> pBuildOffset(meshCount);
        for (size_t infoIdx = 0; infoIdx < meshCount; ++infoIdx) {
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

void RayTracingBasePipeline::createTopLevelAccelerationStructure(VkQueue t_queue,
    TlasCreateInfo t_tlasCreateInfo)
{
    if (t_tlasCreateInfo.instances.empty()) {
        throw std::runtime_error("Cannot create TLAS with zero instances");
    }

    // Buffer for instance data
    VkDeviceSize instancesBufferSize
        = sizeof(VkAccelerationStructureInstanceKHR) * t_tlasCreateInfo.instances.size();

    Buffer instancesBuffer;
    instancesBuffer.create(m_vulkanDevice,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        instancesBufferSize,
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

    if (sizeInfo.accelerationStructureSize == 0) {
        throw std::runtime_error("Cannot create TLAS with zero size");
    }

    if (sizeInfo.buildScratchSize == 0) {
        throw std::runtime_error("Cannot create TLAS scratch buffer with zero size");
    }

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
