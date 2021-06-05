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

void DenoiserOptixPipeline::allocateBuffers(const VkExtent2D& imgSize,
    BufferCuda* t_pixelBufferInRawResult, BufferCuda* t_pixelBufferInAlbedo,
    BufferCuda* t_pixelBufferInNormal, BufferCuda* t_pixelBufferOut)
{
    m_imageSize = imgSize;

    destroy();

    VkDeviceSize bufferSize = m_imageSize.width * m_imageSize.height * 4 * sizeof(float);

    // Using direct method
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    t_pixelBufferInRawResult->create(m_vulkanDevice,
        usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        bufferSize);
    t_pixelBufferInAlbedo->create(m_vulkanDevice,
        usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        bufferSize);
    t_pixelBufferInNormal->create(m_vulkanDevice,
        usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        bufferSize);
    t_pixelBufferOut->create(m_vulkanDevice,
        usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        bufferSize);

    m_pixelBufferInRawResult = t_pixelBufferInRawResult;
    m_pixelBufferInAlbedo = t_pixelBufferInAlbedo;
    m_pixelBufferInNormal = t_pixelBufferInNormal;
    m_pixelBufferOut = t_pixelBufferOut;

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
    float t_blendFactor, uint64_t& t_timelineValue)
{
    try {
        OptixPixelFormat pixelFormat = OPTIX_PIXEL_FORMAT_FLOAT4;
        uint32_t rowStrideInBytes = 4 * sizeof(float) * m_imageSize.width;

        std::vector<OptixImage2D> inputLayer; // Order: RGB, Albedo, Normal

        // RGB
        inputLayer.push_back(OptixImage2D { (CUdeviceptr)m_pixelBufferInRawResult->getCudaPointer(),
            m_imageSize.width,
            m_imageSize.height,
            rowStrideInBytes,
            0,
            pixelFormat });
        // ALBEDO
        if (m_dOptions.inputKind == OPTIX_DENOISER_INPUT_RGB_ALBEDO
            || m_dOptions.inputKind == OPTIX_DENOISER_INPUT_RGB_ALBEDO_NORMAL) {
            inputLayer.push_back(
                OptixImage2D { (CUdeviceptr)m_pixelBufferInAlbedo->getCudaPointer(),
                    m_imageSize.width,
                    m_imageSize.height,
                    rowStrideInBytes,
                    0,
                    pixelFormat });
        }
        // NORMAL
        if (m_dOptions.inputKind == OPTIX_DENOISER_INPUT_RGB_ALBEDO_NORMAL) {
            inputLayer.push_back(
                OptixImage2D { (CUdeviceptr)m_pixelBufferInNormal->getCudaPointer(),
                    m_imageSize.width,
                    m_imageSize.height,
                    rowStrideInBytes,
                    0,
                    pixelFormat });
        }
        OptixImage2D outputLayer = { (CUdeviceptr)m_pixelBufferOut->getCudaPointer(),
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

        if (t_blendFactor < 1.0f) {
            OptixDenoiserParams params {};
            params.denoiseAlpha = 0;
            params.hdrIntensity = m_dIntensity;
            params.hdrAverageColor = m_dAverageRGB;
            params.blendFactor = glm::max(0.0f, t_blendFactor);
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
        }

        cudaExternalSemaphoreSignalParams sigParams {};
        sigParams.flags = 0;
        sigParams.params.fence.value = ++t_timelineValue;
        auto cudaSignalToSemaphore = t_signalTo->getCudaSemaphore();
        cudaSignalExternalSemaphoresAsync(&cudaSignalToSemaphore, &sigParams, 1, stream);
    } catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
    }
}
