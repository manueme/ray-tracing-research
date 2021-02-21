/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "buffer_cuda.h"
#include "utils.hpp"
#include <base_project.h>
#include <cuda.h>
#include <cuda_runtime.h>

BufferCuda::BufferCuda()
    : Buffer()
{
}

void BufferCuda::create(Device* t_device, VkBufferUsageFlags t_usageFlags,
    VkMemoryPropertyFlags t_memoryPropertyFlags, VkDeviceSize t_size, void* t_data)
{
    m_device = t_device->logicalDevice;
    initFunctionPointers();
    Buffer::create(t_device, t_usageFlags, t_memoryPropertyFlags, t_size, t_data);

#ifdef WIN32
    VkMemoryGetWin32HandleInfoKHR handleInfo = {};
    handleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
    handleInfo.memory = memory;
    handleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    CHECK_RESULT(vkGetMemoryWin32HandleKHR(m_device, &handleInfo, &m_handle));
#else
    VkMemoryGetFdInfoKHR handleInfo = {};
    handleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    handleInfo.memory = memory;
    handleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    vkGetMemoryFdKHR(m_device, &handleInfo, &m_handle);
#endif

    VkMemoryRequirements memReqs {};
    vkGetBufferMemoryRequirements(m_device, buffer, &memReqs);

    cudaExternalMemoryHandleDesc cudaExtMemHandleDesc {};
    cudaExtMemHandleDesc.size = memReqs.size;

#ifdef WIN32
    cudaExtMemHandleDesc.type = cudaExternalMemoryHandleTypeOpaqueWin32;
    cudaExtMemHandleDesc.handle.win32.handle = m_handle;
#else
    cudaExtMemHandleDesc.type = cudaExternalMemoryHandleTypeOpaqueFd;
    cudaExtMemHandleDesc.handle.fd = m_handle;
#endif

    cudaExternalMemory_t cudaExtMemVertexBuffer {};
    CUDA_CHECK(cudaImportExternalMemory(&cudaExtMemVertexBuffer, &cudaExtMemHandleDesc))

#ifndef WIN32
    // fd got consumed
    cudaExtMemHandleDesc.handle.fd = -1;
#endif
    cudaExternalMemoryBufferDesc cudaExtBufferDesc {};
    cudaExtBufferDesc.offset = 0;
    cudaExtBufferDesc.size = memReqs.size;
    cudaExtBufferDesc.flags = 0;
    CUDA_CHECK(
        cudaExternalMemoryGetMappedBuffer(&m_cudaPtr, cudaExtMemVertexBuffer, &cudaExtBufferDesc));
}

void BufferCuda::destroy()
{
    Buffer::destroy();
#ifdef WIN32
    CloseHandle(m_handle);
#else
    if (m_handle != -1) {
        close(m_handle);
        handle = -1;
    }
#endif
}
void* BufferCuda::getCudaPointer() { return m_cudaPtr; }

BufferCuda::~BufferCuda() { }
