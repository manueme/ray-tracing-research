/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "denoiser_optix_pipeline.h"
#include "core/buffer.h"
#include "core/device.h"
#include "core/texture.h"
#include "optix.h"
#include "optix_function_table_definition.h"
#include "optix_stubs.h"
#include "semaphore_cuda.h"
#include "utils.hpp"
#include <glm/glm.hpp>

OptixDeviceContext m_optixDevice;

DenoiserOptixPipeline::DenoiserOptixPipeline(Device* t_vulkanDevice)
    : m_device(t_vulkanDevice->logicalDevice)
    , m_vulkanDevice(t_vulkanDevice)
{
    initOptiX();
}

DenoiserOptixPipeline::~DenoiserOptixPipeline() { destroy(); }

int DenoiserOptixPipeline::initOptiX()
{
    // Forces the creation of an implicit CUDA context
    cudaFree(nullptr);

    CUcontext cuCtx;
    CUresult cuRes = cuCtxGetCurrent(&cuCtx);
    if (cuRes != CUDA_SUCCESS) {
        std::cerr << "Error querying current context: error code " << cuRes << "\n";
    }
    OPTIX_CHECK(optixInit())
    OPTIX_CHECK(optixDeviceContextCreate(cuCtx, nullptr, &m_optixDevice))
    OPTIX_CHECK(optixDeviceContextSetLogCallback(m_optixDevice, context_log_cb, nullptr, 4))

    m_dOptions.inputKind = OPTIX_DENOISER_INPUT_RGB_ALBEDO_NORMAL;
    OPTIX_CHECK(optixDenoiserCreate(m_optixDevice, &m_dOptions, &m_denoiser));
    OPTIX_CHECK(optixDenoiserSetModel(m_denoiser, OPTIX_DENOISER_MODEL_KIND_AOV, nullptr, 0));

    return 1;
}

void DenoiserOptixPipeline::allocateBuffers(const VkExtent2D& imgSize)
{
    m_imageSize = imgSize;

    destroy();

    VkDeviceSize bufferSize = m_imageSize.width * m_imageSize.height * 4 * sizeof(float);

    // Using direct method
    VkBufferUsageFlags usage
        = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    m_pixelBufferInRawResult.create(m_vulkanDevice,
        usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        bufferSize);
    m_pixelBufferInAlbedo.create(m_vulkanDevice,
        usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        bufferSize);
    m_pixelBufferInNormal.create(m_vulkanDevice,
        usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        bufferSize);
    m_pixelBufferOut.create(m_vulkanDevice,
        usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        bufferSize);

    // Computing the amount of memory needed to do the denoiser
    OPTIX_CHECK(optixDenoiserComputeMemoryResources(m_denoiser,
        m_imageSize.width,
        m_imageSize.height,
        &m_dSizes))

    CUDA_CHECK(cudaMalloc((void**)&m_dState, m_dSizes.stateSizeInBytes));
    CUDA_CHECK(cudaMalloc((void**)&m_dScratch, m_dSizes.withoutOverlapScratchSizeInBytes));
    CUDA_CHECK(cudaMalloc((void**)&m_dIntensity, sizeof(float)));
    CUDA_CHECK(cudaMalloc((void**)&m_dAverageRGB, 4 * sizeof(float)));

    CUstream stream = nullptr;
    OPTIX_CHECK(optixDenoiserSetup(m_denoiser,
        stream,
        m_imageSize.width,
        m_imageSize.height,
        m_dState,
        m_dSizes.stateSizeInBytes,
        m_dScratch,
        m_dSizes.withoutOverlapScratchSizeInBytes));
}

void DenoiserOptixPipeline::destroy()
{
    m_pixelBufferInRawResult.destroy(); // Closing Handle
    m_pixelBufferInAlbedo.destroy(); // Closing Handle
    m_pixelBufferInNormal.destroy(); // Closing Handle
    m_pixelBufferOut.destroy(); // Closing Handle

    if (m_dState != 0) {
        CUDA_CHECK(cudaFree((void*)m_dState));
    }
    if (m_dScratch != 0) {
        CUDA_CHECK(cudaFree((void*)m_dScratch));
    }
    if (m_dIntensity != 0) {
        CUDA_CHECK(cudaFree((void*)m_dIntensity));
    }
    if (m_dAverageRGB != 0) {
        CUDA_CHECK(cudaFree((void*)m_dAverageRGB));
    }
}

void DenoiserOptixPipeline::denoiseSubmit(SemaphoreCuda* t_waitFor, SemaphoreCuda* t_signalTo,
    uint32_t t_frameIteration, uint64_t& t_timelineValue)
{
    try {
        OptixPixelFormat pixelFormat = OPTIX_PIXEL_FORMAT_FLOAT4;
        uint32_t rowStrideInBytes = 4 * sizeof(float) * m_imageSize.width;

        std::vector<OptixImage2D> inputLayer; // Order: RGB, Albedo, Normal

        // RGB
        inputLayer.push_back(OptixImage2D { (CUdeviceptr)m_pixelBufferInRawResult.getCudaPointer(),
            m_imageSize.width,
            m_imageSize.height,
            rowStrideInBytes,
            0,
            pixelFormat });
        // ALBEDO
        if (m_dOptions.inputKind == OPTIX_DENOISER_INPUT_RGB_ALBEDO
            || m_dOptions.inputKind == OPTIX_DENOISER_INPUT_RGB_ALBEDO_NORMAL) {
            inputLayer.push_back(OptixImage2D { (CUdeviceptr)m_pixelBufferInAlbedo.getCudaPointer(),
                m_imageSize.width,
                m_imageSize.height,
                rowStrideInBytes,
                0,
                pixelFormat });
        }
        // NORMAL
        if (m_dOptions.inputKind == OPTIX_DENOISER_INPUT_RGB_ALBEDO_NORMAL) {
            inputLayer.push_back(OptixImage2D { (CUdeviceptr)m_pixelBufferInNormal.getCudaPointer(),
                m_imageSize.width,
                m_imageSize.height,
                rowStrideInBytes,
                0,
                pixelFormat });
        }
        OptixImage2D outputLayer = { (CUdeviceptr)m_pixelBufferOut.getCudaPointer(),
            m_imageSize.width,
            m_imageSize.height,
            rowStrideInBytes,
            0,
            pixelFormat };

        cudaExternalSemaphoreWaitParams waitParams {};
        waitParams.flags = 0;
        waitParams.params.fence.value = t_timelineValue;
        auto cudaWaitForSemaphore = t_waitFor->getCudaSemaphore();
        cudaWaitExternalSemaphoresAsync(&cudaWaitForSemaphore, &waitParams, 1, nullptr);

        CUstream stream = nullptr;
        OPTIX_CHECK(optixDenoiserComputeIntensity(m_denoiser,
            stream,
            &inputLayer[0],
            m_dIntensity,
            m_dScratch,
            m_dSizes.withoutOverlapScratchSizeInBytes));

        OPTIX_CHECK(optixDenoiserComputeAverageColor(m_denoiser,
            stream,
            &inputLayer[0],
            m_dAverageRGB,
            m_dScratch,
            m_dSizes.withoutOverlapScratchSizeInBytes));

        OptixDenoiserParams params {};
        params.denoiseAlpha = 0;
        params.hdrIntensity = m_dIntensity;
        params.hdrAverageColor = m_dAverageRGB;
        params.blendFactor = glm::min(t_frameIteration / 2000.0f, 1.0f);

        OPTIX_CHECK(optixDenoiserInvoke(m_denoiser,
            stream,
            &params,
            m_dState,
            m_dSizes.stateSizeInBytes,
            inputLayer.data(),
            (uint32_t)inputLayer.size(),
            0,
            0,
            &outputLayer,
            m_dScratch,
            m_dSizes.withoutOverlapScratchSizeInBytes));

        CUDA_CHECK(cudaStreamSynchronize(stream)); // Making sure the denoiser is done

        cudaExternalSemaphoreSignalParams sigParams {};
        sigParams.flags = 0;
        sigParams.params.fence.value = ++t_timelineValue;
        auto cudaSignalToSemaphore = t_signalTo->getCudaSemaphore();
        cudaSignalExternalSemaphoresAsync(&cudaSignalToSemaphore, &sigParams, 1, stream);
    } catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
    }
}

void DenoiserOptixPipeline::buildCommandBufferToImage(
    VkCommandBuffer t_commandBuffer, Texture* imgOut)
{
    const VkBuffer& pixelBufferOut = m_pixelBufferOut.buffer;

    VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    tools::setImageLayout(t_commandBuffer,
        imgOut->getImage(),
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        subresourceRange);

    VkBufferImageCopy bufferCopyRegion = {};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = m_imageSize.width;
    bufferCopyRegion.imageExtent.height = m_imageSize.height;
    bufferCopyRegion.imageExtent.depth = 1;
    bufferCopyRegion.bufferOffset = 0;

    vkCmdCopyBufferToImage(t_commandBuffer,
        pixelBufferOut,
        imgOut->getImage(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &bufferCopyRegion);

    tools::setImageLayout(t_commandBuffer,
        imgOut->getImage(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL,
        subresourceRange);
}

void DenoiserOptixPipeline::buildCommandImageToBuffer(VkCommandBuffer t_commandBuffer,
    Texture* imgInRawResult, Texture* imgInAlbedo, Texture* imgInNormals)
{
    const VkBuffer& pixelBufferIn = m_pixelBufferInRawResult.buffer;

    VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    tools::setImageLayout(t_commandBuffer,
        imgInRawResult->getImage(),
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        subresourceRange);

    VkBufferImageCopy bufferCopyRegion = {};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = m_imageSize.width;
    bufferCopyRegion.imageExtent.height = m_imageSize.height;
    bufferCopyRegion.imageExtent.depth = 1;
    bufferCopyRegion.bufferOffset = 0;

    vkCmdCopyImageToBuffer(t_commandBuffer,
        imgInRawResult->getImage(),
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        pixelBufferIn,
        1,
        &bufferCopyRegion);

    tools::setImageLayout(t_commandBuffer,
        imgInRawResult->getImage(),
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL,
        subresourceRange);
}
