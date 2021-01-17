/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_BASE_RT_PROJECT_H
#define MANUEME_BASE_RT_PROJECT_H

#include "base_project.h"
#include "scene/scene.h"
#include "core/acceleration_structure.h"

class BaseRTProject : public BaseProject {
public:
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
    void initFunctionPointers()
    {
        vkCmdBuildAccelerationStructuresKHR
            = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
                vkGetDeviceProcAddr(m_device, "vkCmdBuildAccelerationStructuresKHR"));
        vkGetAccelerationStructureBuildSizesKHR
            = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
                vkGetDeviceProcAddr(m_device, "vkGetAccelerationStructureBuildSizesKHR"));
        vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
            vkGetDeviceProcAddr(m_device, "vkCmdTraceRaysKHR"));
        vkGetRayTracingShaderGroupHandlesKHR
            = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
                vkGetDeviceProcAddr(m_device, "vkGetRayTracingShaderGroupHandlesKHR"));
        vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
            vkGetDeviceProcAddr(m_device, "vkCreateRayTracingPipelinesKHR"));
    }

    BaseRTProject(std::string t_appName, std::string t_windowTitle, bool t_enableValidation = false);
    ~BaseRTProject();

protected:
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rayTracingPipelineProperties {};

    // Bottom level acceleration structure
    std::vector<AccelerationStructure> m_bottomLevelAS;
    // Top level acceleration structure
    AccelerationStructure m_topLevelAS;

    VkDeviceSize copyRTShaderIdentifier(
        uint8_t* t_data, const uint8_t* t_shaderHandleStorage, uint32_t t_groupIndex) const;
    void createBottomLevelAccelerationStructure(const std::vector<BlasCreateInfo>& t_blases);
    void createTopLevelAccelerationStructure(TlasCreateInfo t_tlasCreateInfo);

    // Device extra features
    VkPhysicalDeviceBufferDeviceAddressFeatures m_bufferDeviceAddressFeatures {};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR m_rayTracingPipelineFeatures {};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR m_accelerationStructureFeatures {};
    VkPhysicalDeviceDescriptorIndexingFeaturesEXT m_descriptorIndexingFeatures {};
    void getDeviceRayTracingProperties();


    void getEnabledFeatures() override;
    void prepare() override;
    void createRTScene(const std::string& t_modelPath, SceneVertexLayout t_vertexLayout, VkPipeline* t_pipeline = nullptr);
};

#endif // MANUEME_BASE_RT_PROJECT_H
