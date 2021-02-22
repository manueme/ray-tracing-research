/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef RAY_TRACING_DENOISER_OPTIX_PIPELINE_H
#define RAY_TRACING_DENOISER_OPTIX_PIPELINE_H

#include "buffer_cuda.h"
#include "optix_types.h"
#include "vulkan/vulkan_core.h"
#include <array>
#include <cuda.h>
#include <cuda_runtime.h>

class Texture;
class SemaphoreCuda;

class DenoiserOptixPipeline {

public:
    DenoiserOptixPipeline(Device* t_vulkanDevice);

    ~DenoiserOptixPipeline();

    void destroy();

    void allocateBuffers(const VkExtent2D& imgSize);

    void buildCommandBufferToImage(VkCommandBuffer t_commandBuffer, Texture* imgOut);
    void buildCommandImageToBuffer(VkCommandBuffer t_commandBuffer, Texture* imgInRawResult,
        Texture* imgInAlbedo, Texture* imgInNormals);

    void denoiseSubmit(
        SemaphoreCuda* t_waitFor, SemaphoreCuda* t_signalTo, uint32_t t_frameIteration, uint64_t& t_timelineValue);

private:
    Device* m_vulkanDevice;
    VkDevice m_device;

    OptixDenoiser m_denoiser { nullptr };
    OptixDenoiserOptions m_dOptions {};
    OptixDenoiserSizes m_dSizes {};
    CUdeviceptr m_dState { 0 };
    CUdeviceptr m_dScratch { 0 };
    CUdeviceptr m_dIntensity { 0 };
    CUdeviceptr m_dAverageRGB { 0 };

    // Holding the Buffer for Cuda interop
    VkExtent2D m_imageSize;
    BufferCuda m_pixelBufferInRawResult;
    BufferCuda m_pixelBufferInAlbedo;
    BufferCuda m_pixelBufferInNormal;
    BufferCuda m_pixelBufferOut;

    int initOptiX();
};

#endif // RAY_TRACING_DENOISER_OPTIX_PIPELINE_H
