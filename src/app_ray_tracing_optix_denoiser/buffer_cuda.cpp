/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "buffer_cuda.h"
#include "utils.hpp"
#include <base_project.h>

BufferCuda::BufferCuda()
    : Buffer()
{
}

void BufferCuda::create(Device* t_device, VkBufferUsageFlags t_usageFlags,
    VkMemoryPropertyFlags t_memoryPropertyFlags, VkDeviceSize t_size, void* t_data,
    void* t_createInfoNext, void* t_allocationInfoNext)
{
    // Adding External memory flag for creation
    VkExternalMemoryBufferCreateInfo infoEx = {};
    infoEx.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
#ifdef WIN32
    infoEx.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
    infoEx.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif
    infoEx.pNext = t_createInfoNext;
    // ----
    // Adding External memory flag for allocation
    VkExportMemoryAllocateInfo memoryHandleEx = {};
    memoryHandleEx.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
#ifdef WIN32
    memoryHandleEx.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
    memoryHandleEx.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif
    memoryHandleEx.pNext = t_allocationInfoNext;
    // ---
    Buffer::create(t_device,
        t_usageFlags,
        t_memoryPropertyFlags,
        t_size,
        t_data,
        &infoEx,
        &memoryHandleEx);

#ifdef WIN32
    VkMemoryGetWin32HandleInfoKHR handleInfo = {};
    handleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
    handleInfo.memory = memory;
    handleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    CHECK_RESULT(vkGetMemoryWin32HandleKHR(device, &handleInfo, &m_handle));
#else
    VkMemoryGetFdInfoKHR handleInfo = {};
    handleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    handleInfo.memory = memory;
    handleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    vkGetMemoryFdKHR(m_device, &handleInfo, &m_handle);
#endif
    cudaExternalMemoryHandleDesc cudaExtMemHandleDesc {};
    cudaExtMemHandleDesc.size = size;

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
    cudaExtBufferDesc.size = size;
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

void BufferCuda::initFunctionPointers()
{
    Buffer::initFunctionPointers();
    vkGetMemoryWin32HandleKHR = reinterpret_cast<PFN_vkGetMemoryWin32HandleKHR>(
        vkGetDeviceProcAddr(device, "vkGetMemoryWin32HandleKHR"));
}
