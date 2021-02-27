/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "semaphore_cuda.h"
#include "tools/tools.h"
#include "utils.hpp"

SemaphoreCuda::SemaphoreCuda() { }

void SemaphoreCuda::create(VkDevice t_device)
{
    m_device = t_device;
    initFunctionPointers();
#ifdef WIN32
    auto handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
    auto handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

    VkSemaphoreTypeCreateInfo timelineCreateInfo = {};
    timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineCreateInfo.initialValue = 0;

    VkSemaphoreCreateInfo sci = {};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    sci.pNext = &timelineCreateInfo;
    VkExportSemaphoreCreateInfo esci = {};
    esci.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
    esci.pNext = &timelineCreateInfo;
    sci.pNext = &esci;
    esci.handleTypes = handleType;
    CHECK_RESULT(vkCreateSemaphore(m_device, &sci, nullptr, &m_semaphore))

#ifdef WIN32
    VkSemaphoreGetWin32HandleInfoKHR handleInfo = {};
    handleInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
    handleInfo.semaphore = m_semaphore;
    handleInfo.handleType = handleType;
    CHECK_RESULT(vkGetSemaphoreWin32HandleKHR(m_device, &handleInfo, &m_handle))

    cudaExternalSemaphoreHandleDesc externalSemaphoreHandleDesc;
    std::memset(&externalSemaphoreHandleDesc, 0, sizeof(externalSemaphoreHandleDesc));
    externalSemaphoreHandleDesc.flags = 0;
    externalSemaphoreHandleDesc.type = cudaExternalSemaphoreHandleTypeD3D12Fence;
    externalSemaphoreHandleDesc.handle.win32.handle = (void*)m_handle;
#else
    VkSemaphoreGetFdInfoKHR handleInfo = {};
    handleInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
    handleInfo.semaphore = m_semaphore;
    handleInfo.handleType = handleType;
    vkGetSemaphoreFdKHR(m_device, &handleInfo, &m_handle);

    cudaExternalSemaphoreHandleDesc externalSemaphoreHandleDesc;
    std::memset(&externalSemaphoreHandleDesc, 0, sizeof(externalSemaphoreHandleDesc));
    externalSemaphoreHandleDesc.flags = 0;
    externalSemaphoreHandleDesc.type = cudaExternalSemaphoreHandleTypeOpaqueFd;
    externalSemaphoreHandleDesc.handle.fd = m_handle;
#endif

    CUDA_CHECK(cudaImportExternalSemaphore(&m_cuSemaphore, &externalSemaphoreHandleDesc));
}

VkResult SemaphoreCuda::waitSemaphore(uint64_t timeout, uint64_t& t_timelineValue)
{
    VkSemaphoreWaitInfo waitInfo { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &m_semaphore;
    waitInfo.pValues = &t_timelineValue;
    return vkWaitSemaphoresKHR(m_device, &waitInfo, 10000);
}

void SemaphoreCuda::destroy() { vkDestroySemaphore(m_device, m_semaphore, nullptr); }

cudaExternalSemaphore_t SemaphoreCuda::getCudaSemaphore() { return m_cuSemaphore; }

VkSemaphore SemaphoreCuda::getVulkanSemaphore() { return m_semaphore; }

void SemaphoreCuda::initFunctionPointers()
{
#ifdef WIN32
    vkGetSemaphoreWin32HandleKHR = reinterpret_cast<PFN_vkGetSemaphoreWin32HandleKHR>(
        vkGetDeviceProcAddr(m_device, "vkGetSemaphoreWin32HandleKHR"));
#else
    vkGetSemaphoreFdKHR = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(
        vkGetDeviceProcAddr(m_device, "vkGetSemaphoreFdKHR"));
#endif
    vkWaitSemaphoresKHR = reinterpret_cast<PFN_vkWaitSemaphoresKHR>(
        vkGetDeviceProcAddr(m_device, "vkWaitSemaphoresKHR"));
}
