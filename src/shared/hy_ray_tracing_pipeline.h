/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef SHARED_HYBRID_RAY_TRACING_PIPELINE_H
#define SHARED_HYBRID_RAY_TRACING_PIPELINE_H

#include "core/acceleration_structure.h"
#include "core/buffer.h"
#include "scene/scene.h"
#include "vulkan/vulkan_core.h"

class Texture;
class Device;

class HyRayTracingPipeline {
public:
    HyRayTracingPipeline(Device* t_vulkanDevice, uint32_t t_maxDepth, uint32_t t_sampleCount);
    ~HyRayTracingPipeline();

    void buildCommandBuffer(
        uint32_t t_index, VkCommandBuffer t_commandBuffer, uint32_t t_width, uint32_t t_height);

    void createDescriptorSetsLayout(Scene* t_scene);

    void createPipeline(std::vector<VkPipelineShaderStageCreateInfo> t_shaderStages,
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> t_shaderGroups);

    void createDescriptorSets(VkDescriptorPool t_descriptorPool, Scene* t_scene,
        uint32_t t_swapChainCount, std::vector<Buffer>& t_sceneBuffers, Buffer* t_instancesBuffer,
        Buffer* t_lightsBuffer, Buffer* t_materialsBuffer);

    void updateResultImageDescriptorSets(uint32_t t_index, Texture* t_offscreenColor,
        Texture* t_offscreenNormals, Texture* t_offscreenReflectRefractMap,
        Texture* t_offscreenDepth, Texture* t_result);

    Scene* createRTScene(
        VkQueue t_queue, const std::string& t_modelPath, SceneVertexLayout t_vertexLayout);

private:
    Device* m_vulkanDevice;
    VkDevice m_device;

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

    VkPipeline m_pipeline;
    VkPipelineLayout m_pipelineLayout;
    struct {
        VkDescriptorSet set0AccelerationStructure;
        std::vector<VkDescriptorSet> set1Scene;
        VkDescriptorSet set2Geometry;
        VkDescriptorSet set3Materials;
        VkDescriptorSet set4Lights;
        std::vector<VkDescriptorSet> set5OffscreenImages;
        std::vector<VkDescriptorSet> set6StorageImages;
    } m_descriptorSets;
    struct {
        VkDescriptorSetLayout set0AccelerationStructure;
        VkDescriptorSetLayout set1Scene;
        VkDescriptorSetLayout set2Geometry;
        VkDescriptorSetLayout set3Materials;
        VkDescriptorSetLayout set4Lights;
        VkDescriptorSetLayout set5OffscreenImages;
        VkDescriptorSetLayout set6StorageImages;
    } m_descriptorSetLayouts;

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

#endif // SHARED_HYBRID_RAY_TRACING_PIPELINE_H
