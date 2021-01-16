/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "base_rt_project.h"

BaseRTProject::BaseRTProject(
    std::string t_appName, std::string t_windowTitle, bool t_enableValidation)
    : BaseProject(t_appName, t_windowTitle, t_enableValidation)
{
    m_enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    m_enabledFeatures.fragmentStoresAndAtomics = VK_TRUE;
}

void BaseRTProject::getDeviceRayTracingProperties()
{
    m_rayTracingPipelineProperties.sType
        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 deviceProps2 {};
    deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProps2.pNext = &m_rayTracingPipelineProperties;
    vkGetPhysicalDeviceProperties2(m_physicalDevice, &deviceProps2);

    initFunctionPointers();
}

void BaseRTProject::getEnabledFeatures() {
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

void BaseRTProject::prepare() {
    BaseProject::prepare();

    getDeviceRayTracingProperties();
}

BaseRTProject::~BaseRTProject() = default;
