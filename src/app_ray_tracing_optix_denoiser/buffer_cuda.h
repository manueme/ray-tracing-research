/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_BUFFER_CUDA_H
#define MANUEME_BUFFER_CUDA_H

#include "core/buffer.h"
#include "vulkan/vulkan_core.h"
#include <cuda.h>
#include <cuda_runtime.h>
#ifdef WIN32
#include <windows.h>
// nolint
#include "vulkan/vulkan_win32.h"
#endif

class BufferCuda : public Buffer {
public:
    BufferCuda();
    ~BufferCuda();

    PFN_vkGetMemoryWin32HandleKHR vkGetMemoryWin32HandleKHR;
    void initFunctionPointers() override;

    void create(Device* t_device, VkBufferUsageFlags t_usageFlags,
        VkMemoryPropertyFlags t_memoryPropertyFlags, VkDeviceSize t_size, void* t_data = nullptr,
        void* t_createInfoNext = nullptr, void* t_allocationInfoNext = nullptr);

    void destroy() override;

    void* getCudaPointer();

private:
    // Extra for Cuda
#ifdef WIN32
    HANDLE m_handle = nullptr; // The Win32 handle
#else
    int m_handle = -1;
#endif
    void* m_cudaPtr = nullptr;
};

#endif // MANUEME_BUFFER_CUDA_H
