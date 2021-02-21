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
#include <windows.h>
// nolint
#include "vulkan/vulkan_win32.h"
#endif

class SemaphoreCuda {
public:
    SemaphoreCuda();
    PFN_vkGetSemaphoreWin32HandleKHR vkGetSemaphoreWin32HandleKHR;
    void initFunctionPointers()
    {
        vkGetSemaphoreWin32HandleKHR = reinterpret_cast<PFN_vkGetSemaphoreWin32HandleKHR>(
            vkGetDeviceProcAddr(m_device, "vkGetSemaphoreWin32HandleKHR"));
    }

    void create(VkDevice t_device);
    void destroy();

    cudaExternalSemaphore_t getCudaSemaphore();
    VkSemaphore getVulkanSemaphore();

private:
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
