/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "base_rt_project.h"
#include "shaders/constants.h"

BaseRTProject::BaseRTProject(
    std::string t_appName, std::string t_windowTitle, bool t_enableValidation)
    : BaseProject(t_appName, t_windowTitle, t_enableValidation)
{
    m_enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    m_enabledFeatures.fragmentStoresAndAtomics = VK_TRUE;
}

void BaseRTProject::getEnabledFeatures()
{
    BaseProject::getEnabledFeatures();

    m_enabledDeviceExtensions.emplace_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    m_enabledDeviceExtensions.emplace_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    m_enabledDeviceExtensions.emplace_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    m_enabledDeviceExtensions.emplace_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    m_enabledDeviceExtensions.emplace_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
    m_enabledDeviceExtensions.emplace_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    m_enabledDeviceExtensions.emplace_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

    // Required for VK_KHR_ray_tracing_pipeline
    m_enabledDeviceExtensions.emplace_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
    // Required by VK_KHR_spirv_1_4
    m_enabledDeviceExtensions.emplace_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);

    m_bufferDeviceAddressFeatures.sType
        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    m_bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

    m_rayTracingPipelineFeatures.sType
        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    m_rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
    m_rayTracingPipelineFeatures.pNext = &m_bufferDeviceAddressFeatures;

    m_accelerationStructureFeatures.sType
        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    m_accelerationStructureFeatures.accelerationStructure = VK_TRUE;
    m_accelerationStructureFeatures.pNext = &m_rayTracingPipelineFeatures;

    m_descriptorIndexingFeatures.sType
        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    m_descriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
    m_descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    m_descriptorIndexingFeatures.pNext = &m_accelerationStructureFeatures;

    m_deviceCreatedNextChain = &m_descriptorIndexingFeatures;
}

/*
    The bottom level acceleration structure contains the scene's geometry
   (vertices, triangles)
*/
void BaseRTProject::createBottomLevelAccelerationStructure(
    const std::vector<BlasCreateInfo>& t_blases)
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
    m_vulkanDevice->flushCommandBuffer(cmdBuffer, m_queue);
    scratchBuffer.destroy();
}

/*
    The top level acceleration structure contains the scene's object instances
*/
void BaseRTProject::createTopLevelAccelerationStructure(TlasCreateInfo t_tlasCreateInfo)
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

    m_vulkanDevice->flushCommandBuffer(cmdBuffer, m_queue);
    scratchBuffer.destroy();
    instancesBuffer.destroy();
}

void BaseRTProject::createRTScene(
    const std::string& t_modelPath, SceneVertexLayout t_vertexLayout, VkPipeline* t_pipeline)
{
    // Models
    SceneCreateInfo modelCreateInfo(glm::vec3(1.0f), glm::vec3(1.0f), glm::vec3(0.0f));
    modelCreateInfo.memoryPropertyFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    m_scene = new Scene();
    m_scene->loadFromFile(t_modelPath,
        t_vertexLayout,
        &modelCreateInfo,
        m_vulkanDevice,
        m_queue,
        t_pipeline);
    auto camera = m_scene->getCamera();
    camera->setMovementSpeed(500.0f);
    camera->setRotationSpeed(0.5f);
    camera->setPerspective(60.0f,
        static_cast<float>(m_width) / static_cast<float>(m_height),
        0.1f,
        5000.0f);

    // One Geometry per blas for this scene
    VkAccelerationStructureGeometryKHR geometry {};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.geometry.triangles.sType
        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry.geometry.triangles.vertexData.deviceAddress = m_scene->vertices.getDeviceAddress();
    geometry.geometry.triangles.vertexStride = t_vertexLayout.stride();
    geometry.geometry.triangles.maxVertex = m_scene->vertexCount;
    geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geometry.geometry.triangles.indexData.deviceAddress = m_scene->indices.getDeviceAddress();
    geometry.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;

    std::vector<BlasCreateInfo> blases;
    for (auto i = 0; i < m_scene->meshes.size(); i++) {
        const auto mesh = m_scene->meshes[i];
        VkAccelerationStructureBuildRangeInfoKHR meshOffsetInfo {};
        meshOffsetInfo.primitiveCount = mesh.getIndexCount() / 3;
        meshOffsetInfo.primitiveOffset = mesh.getIndexOffset();
        meshOffsetInfo.firstVertex = mesh.getVertexBase();

        BlasCreateInfo blas; // One blas per mesh. All the meshes are part of the same geometry with
        // different offsets
        blas.geomery = { geometry };
        blas.meshes = { meshOffsetInfo };
        blases.emplace_back(blas);

        m_scene->createMeshInstance(i, i);
    }
    createBottomLevelAccelerationStructure(blases);

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
    for (auto meshInstance : m_scene->instances) {
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
    createTopLevelAccelerationStructure(geometryInstances);
}

VkDeviceSize BaseRTProject::copyRTShaderIdentifier(
    uint8_t* t_data, const uint8_t* t_shaderHandleStorage, uint32_t t_groupIndex) const
{
    const uint32_t shaderGroupHandleSize = m_rayTracingPipelineProperties.shaderGroupHandleSize;
    memcpy(t_data,
        t_shaderHandleStorage + t_groupIndex * shaderGroupHandleSize,
        shaderGroupHandleSize);
    return shaderGroupHandleSize;
}

void BaseRTProject::getDeviceRayTracingProperties()
{
    m_rayTracingPipelineProperties.sType
        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 deviceProps2 {};
    deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProps2.pNext = &m_rayTracingPipelineProperties;
    vkGetPhysicalDeviceProperties2(m_physicalDevice, &deviceProps2);
}

void BaseRTProject::prepare()
{
    BaseProject::prepare();
    getDeviceRayTracingProperties();
    initFunctionPointers();
}

BaseRTProject::~BaseRTProject()
{
    m_topLevelAS.destroy();
    for (auto& blas : m_bottomLevelAS) {
        blas.destroy();
    }
};
