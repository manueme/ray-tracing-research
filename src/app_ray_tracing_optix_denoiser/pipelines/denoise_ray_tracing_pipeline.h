/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef DENOISE_RAY_TRACING_PIPELINE_H
#define DENOISE_RAY_TRACING_PIPELINE_H

#include "core/acceleration_structure.h"
#include "core/buffer.h"
#include "ray_tracing_base_pipeline.h"
#include "scene/scene.h"
#include "vulkan/vulkan_core.h"

class Texture;
class Device;

class DenoiseRayTracingPipeline : public RayTracingBasePipeline {
public:
    DenoiseRayTracingPipeline(Device* t_vulkanDevice, uint32_t t_maxDepth, uint32_t t_sampleCount);
    ~DenoiseRayTracingPipeline();

    void buildCommandBuffer(
        VkCommandBuffer t_commandBuffer, uint32_t t_width, uint32_t t_height);

    void createDescriptorSetsLayout(Scene* t_scene) override;

    void createDescriptorSets(VkDescriptorPool t_descriptorPool, Scene* t_scene,
        Buffer* t_sceneBuffer, Buffer* t_instancesBuffer, Buffer* t_lightsBuffer,
        Buffer* t_materialsBuffer);

    void updateResultImageDescriptorSets(Texture* t_depthMap, Buffer* t_normalsBuffer,
        Buffer* t_albedoBuffer, Buffer* t_outImageBuffer);
private:
    struct {
        VkDescriptorSet set0AccelerationStructure;
        VkDescriptorSet set1Scene;
        VkDescriptorSet set2Geometry;
        VkDescriptorSet set3Materials;
        VkDescriptorSet set4Lights;
        VkDescriptorSet set5ResultImages;
        VkDescriptorSet set6ResultBuffers;
    } m_descriptorSets;
    struct {
        VkDescriptorSetLayout set0AccelerationStructure;
        VkDescriptorSetLayout set1Scene;
        VkDescriptorSetLayout set2Geometry;
        VkDescriptorSetLayout set3Materials;
        VkDescriptorSetLayout set4Lights;
        VkDescriptorSetLayout set5ResultImages;
        VkDescriptorSetLayout set6ResultBuffers;
    } m_descriptorSetLayouts;

    void createShaderBindingTable() override;
};

#endif // DENOISE_RAY_TRACING_PIPELINE_H
