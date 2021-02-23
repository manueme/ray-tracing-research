/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef BASE_RAY_TRACING_PIPELINE_H
#define BASE_RAY_TRACING_PIPELINE_H

#include "core/acceleration_structure.h"
#include "core/buffer.h"
#include "scene/scene.h"
#include "vulkan/vulkan_core.h"

class Texture;
class Device;

class RayTracingBasePipeline {
public:
    virtual void createDescriptorSetsLayout(Scene* t_scene) = 0;

    void createPipeline(std::vector<VkPipelineShaderStageCreateInfo> t_shaderStages,
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> t_shaderGroups);

    Scene* createRTScene(
        VkQueue t_queue, const std::string& t_modelPath, SceneVertexLayout t_vertexLayout);

protected:
    RayTracingBasePipeline(Device* t_vulkanDevice, uint32_t t_maxDepth, uint32_t t_sampleCount);
    ~RayTracingBasePipeline();

    VkDevice m_device;
    Device* m_vulkanDevice;

    VkPipeline m_pipeline;
    VkPipelineLayout m_pipelineLayout;

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

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rayTracingPipelineProperties;
    void getDeviceRayTracingProperties();

    // Bottom level acceleration structure
    std::vector<AccelerationStructure> m_bottomLevelAS;
    void createBottomLevelAccelerationStructure(
        VkQueue t_queue, const std::vector<BlasCreateInfo>& t_blases);
    // Top level acceleration structure
    AccelerationStructure m_topLevelAS;
    void createTopLevelAccelerationStructure(VkQueue t_queue, TlasCreateInfo t_tlasCreateInfo);

    // Push constant sent to the path tracer
    struct PathTracerParameters {
        uint32_t maxDepth; // Max depth
        uint32_t samples; // samples per frame
    } m_pathTracerParams;

    Buffer m_shaderBindingTable;
    virtual void createShaderBindingTable() = 0;
    VkDeviceSize copyRTShaderIdentifier(
        uint8_t* t_data, const uint8_t* t_shaderHandleStorage, uint32_t t_groupIndex) const;
};

#endif // BASE_RAY_TRACING_PIPELINE_H
