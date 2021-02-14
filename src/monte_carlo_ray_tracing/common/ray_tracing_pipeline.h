/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef COMMON_RAY_TRACING_PIPELINE_H
#define COMMON_RAY_TRACING_PIPELINE_H

#include "core/buffer.h"
#include "vulkan/vulkan_core.h"

class Scene;
class Texture;
class Device;

class RayTracingPipeline {
public:
    RayTracingPipeline(Device* t_vulkanDevice,
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR t_rayTracingPipelineProperties);
    ~RayTracingPipeline();

    void buildCommandBuffer(VkCommandBuffer t_commandBuffer, uint32_t t_width, uint32_t t_height);

    void createDescriptorSetsLayout(Scene* t_scene);

    void createPipeline(std::vector<VkPipelineShaderStageCreateInfo> t_shaderStages,
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> t_shaderGroups);

    void createDescriptorSets(VkDescriptorPool t_descriptorPool,
        VkAccelerationStructureKHR t_accelerationStructure, Scene* t_scene, Buffer* t_sceneBuffer,
        Buffer* t_instancesBuffer, Buffer* t_lightsBuffer, Buffer* t_materialsBuffer);

    void updateResultImageDescriptorSets(
        Texture* t_result, Texture* t_depthMap, Texture* t_normalMap, Texture* t_albedo);

private:
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

    Device* m_vulkanDevice;
    VkDevice m_device;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rayTracingPipelineProperties;

    VkPipeline m_pipeline;
    VkPipelineLayout m_pipelineLayout;
    struct {
        VkDescriptorSet set0AccelerationStructure;
        VkDescriptorSet set1Scene;
        VkDescriptorSet set2Geometry;
        VkDescriptorSet set3Materials;
        VkDescriptorSet set4Lights;
        VkDescriptorSet set5ResultImage;
    } m_descriptorSets;
    struct {
        VkDescriptorSetLayout set0AccelerationStructure;
        VkDescriptorSetLayout set1Scene;
        VkDescriptorSetLayout set2Geometry;
        VkDescriptorSetLayout set3Materials;
        VkDescriptorSetLayout set4Lights;
        VkDescriptorSetLayout set5ResultImage;
    } m_descriptorSetLayouts;

    const uint32_t m_ray_tracer_depth = 4;
    const uint32_t m_ray_tracer_samples = 1;
    // Push constant sent to the path tracer
    struct PathTracerParameters {
        uint32_t maxDepth; // Max depth
        uint32_t samples; // samples per frame
    } m_pathTracerParams;

    Buffer m_shaderBindingTable;
    void createShaderBindingTable();
    VkDeviceSize copyRTShaderIdentifier(
        uint8_t* t_data, const uint8_t* t_shaderHandleStorage, uint32_t t_groupIndex) const;
};

#endif // COMMON_RAY_TRACING_PIPELINE_H
