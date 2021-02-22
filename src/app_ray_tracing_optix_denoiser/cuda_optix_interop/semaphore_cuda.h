/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_SEMAPHORE_CUDA_H
#define MANUEME_SEMAPHORE_CUDA_H

#include "vulkan/vulkan_core.h"
#include <cuda.h>
#include <cuda_runtime.h>
#ifdef WIN32
#include <Windows.h>
// nolint
#include "vulkan/vulkan_win32.h"
#endif

class SemaphoreCuda {
public:
    SemaphoreCuda();

    void create(VkDevice t_device);
    void destroy();

    cudaExternalSemaphore_t getCudaSemaphore();
    VkSemaphore getVulkanSemaphore();
    VkResult waitSemaphore(uint64_t timeout, uint64_t& t_timelineValue);

private:
    PFN_vkGetSemaphoreWin32HandleKHR vkGetSemaphoreWin32HandleKHR;
    PFN_vkWaitSemaphoresKHR vkWaitSemaphoresKHR;
    void initFunctionPointers();
    VkDevice m_device;
    VkSemaphore m_semaphore;
#ifdef WIN32
    HANDLE m_handle { INVALID_HANDLE_VALUE };
#else
    int m_handle { -1 };
#endif
    cudaExternalSemaphore_t m_cuSemaphore;
};

#endif // MANUEME_SEMAPHORE_CUDA_H
