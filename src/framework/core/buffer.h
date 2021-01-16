/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_BUFFER_H
#define MANUEME_BUFFER_H

#include <vector>

#include "../tools/tools.h"
#include "vulkan/vulkan.h"

class Device;

/**
 * @brief Encapsulates access to a Vulkan buffer backed up by device memory
 */
class Buffer {
public:
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
    void initFunctionPointers()
    {
        vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(
            vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR"));
    }

    Buffer();
    ~Buffer();

    /**
     * Create a buffer on the device
     *
     * @param t_usageFlags Usage flag bitmask for the buffer (i.e. index, vertex,
     * uniform buffer)
     * @param t_memoryPropertyFlags Memory properties for this buffer (i.e. device
     * local, host visible, coherent)
     * @param t_buffer Pointer to a vk::Vulkan buffer object
     * @param t_size Size of the buffer in byes
     * @param t_data Pointer to the data that should be copied to the buffer after
     * creation (optional, if not set, no data is copied over)
     *
     */
    void create(Device* t_device, VkBufferUsageFlags t_usageFlags,
        VkMemoryPropertyFlags t_memoryPropertyFlags, VkDeviceSize t_size, void* t_data = nullptr);

    VkDevice device;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDescriptorBufferInfo descriptor;
    VkDeviceSize size = 0;
    VkDeviceSize alignment = 0;
    void* mapped = nullptr;

    /**
     * Map a memory range of this buffer. If successful, mapped points to the
     * specified buffer range.
     *
     * @param t_size (Optional) Size of the memory range to map. Pass
     * VK_WHOLE_SIZE to map the complete buffer range.
     * @param t_offset (Optional) Byte offset from beginning
     *
     * @return VkResult of the buffer mapping call
     */
    VkResult map(VkDeviceSize t_size = VK_WHOLE_SIZE, VkDeviceSize t_offset = 0);

    /**
     * Unmap a mapped memory range
     *
     * @note Does not return a result as vkUnmapMemory can't fail
     */
    void unmap();

    /**
     * Attach the allocated memory block to the buffer
     *
     * @param t_offset (Optional) Byte offset (from the beginning) for the memory
     * region to bind
     *
     * @return VkResult of the bindBufferMemory call
     */
    VkResult bind(VkDeviceSize t_offset = 0);

    /**
     * Setup the default descriptor for this buffer
     *
     * @param t_size (Optional) Size of the memory range of the descriptor
     * @param t_offset (Optional) Byte offset from beginning
     *
     */
    void setupDescriptor(VkDeviceSize t_size = VK_WHOLE_SIZE, VkDeviceSize t_offset = 0);

    /**
     * Copies the specified data to the mapped buffer
     *
     * @param t_data Pointer to the data to copy
     * @param t_size Size of the data to copy in machine units
     *
     */
    void copyTo(void* t_data, VkDeviceSize t_size);

    /**
     * Flush a memory range of the buffer to make it visible to the device
     *
     * @note Only required for non-coherent memory
     *
     * @param t_size (Optional) Size of the memory range to flush. Pass
     * VK_WHOLE_SIZE to flush the complete buffer range.
     * @param t_offset (Optional) Byte offset from beginning
     *
     * @return VkResult of the flush call
     */
    VkResult flush(VkDeviceSize t_size = VK_WHOLE_SIZE, VkDeviceSize t_offset = 0);

    /**
     * Invalidate a memory range of the buffer to make it visible to the host
     *
     * @note Only required for non-coherent memory
     *
     * @param t_size (Optional) Size of the memory range to invalidate. Pass
     * VK_WHOLE_SIZE to invalidate the complete buffer range.
     * @param t_offset (Optional) Byte offset from beginning
     *
     * @return VkResult of the invalidate call
     */
    VkResult invalidate(VkDeviceSize t_size = VK_WHOLE_SIZE, VkDeviceSize t_offset = 0);

    VkDeviceAddress getDeviceAddress();

    /**
     * Release all Vulkan resources held by this buffer
     */
    void destroy();

private:
    /** @brief Usage flags to be filled by external source at buffer creation (to
     * query at some later point) */
    VkBufferUsageFlags usageFlags;
    /** @brief Memory propertys flags to be filled by external source at buffer
     * creation (to query at some later point) */
    VkMemoryPropertyFlags memoryPropertyFlags;
};

#endif // MANUEME_BUFFER_H
