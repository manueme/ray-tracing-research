/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef SHARED_HYBRID_RAY_TRACING_PIPELINE_H
#define SHARED_HYBRID_RAY_TRACING_PIPELINE_H

#include "core/acceleration_structure.h"
#include "core/buffer.h"
#include "ray_tracing_base_pipeline.h"
#include "scene/scene.h"
#include "vulkan/vulkan_core.h"

class Texture;
class Device;

class HyRayTracingPipeline : public RayTracingBasePipeline {
public:
    HyRayTracingPipeline(Device* t_vulkanDevice, uint32_t t_maxDepth, uint32_t t_sampleCount);
    ~HyRayTracingPipeline();

    void buildCommandBuffer(
        uint32_t t_index, VkCommandBuffer t_commandBuffer, uint32_t t_width, uint32_t t_height);

    void createDescriptorSetsLayout(Scene* t_scene) override;

    void createDescriptorSets(VkDescriptorPool t_descriptorPool, Scene* t_scene,
        uint32_t t_swapChainCount, std::vector<Buffer>& t_sceneBuffers, Buffer* t_instancesBuffer,
        Buffer* t_lightsBuffer, Buffer* t_materialsBuffer);

    void updateResultImageDescriptorSets(uint32_t t_index, Texture* t_offscreenColor,
        Texture* t_offscreenNormals, Texture* t_offscreenReflectRefractMap,
        Texture* t_offscreenDepth, Texture* t_result);

private:
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

    void createShaderBindingTable() override;
};

#endif // SHARED_HYBRID_RAY_TRACING_PIPELINE_H
