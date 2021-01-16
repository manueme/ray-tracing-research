/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "buffer.h"

#include "device.h"

Buffer::Buffer()
{
};

Buffer::~Buffer() = default;

void Buffer::create(Device* t_device, VkBufferUsageFlags t_usageFlags,
    VkMemoryPropertyFlags t_memoryPropertyFlags, VkDeviceSize t_size, void* t_data)
{
    this->device = t_device->logicalDevice;
    initFunctionPointers();

    // Create the buffer handle
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.usage = t_usageFlags;
    bufferCreateInfo.size = t_size;
    VKM_CHECK_RESULT(vkCreateBuffer(this->device, &bufferCreateInfo, nullptr, &this->buffer));

    // Create the memory backing up the buffer handle
    VkMemoryRequirements memReqs;
    VkMemoryAllocateInfo memAlloc = {};
    memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
    if (t_usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
        allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
        memAlloc.pNext = &allocFlagsInfo;
    }
    vkGetBufferMemoryRequirements(this->device, this->buffer, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    // Find a memory type index that fits the properties of the buffer
    memAlloc.memoryTypeIndex
        = t_device->getMemoryType(memReqs.memoryTypeBits, t_memoryPropertyFlags);
    VKM_CHECK_RESULT(vkAllocateMemory(this->device, &memAlloc, nullptr, &this->memory));

    this->alignment = memReqs.alignment;
    this->size = t_size;
    this->usageFlags = t_usageFlags;
    this->memoryPropertyFlags = t_memoryPropertyFlags;

    // If a pointer to the buffer data has been passed, map the buffer and copy
    // over the data
    if (t_data != nullptr) {
        VKM_CHECK_RESULT(this->map());
        memcpy(this->mapped, t_data, t_size);
        if ((t_memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
            this->flush();
        }

        this->unmap();
    }

    // Initialize a default descriptor that covers the whole buffer size
    this->setupDescriptor();

    // Attach the memory to the buffer object
    VKM_CHECK_RESULT(this->bind());
}

VkResult Buffer::map(VkDeviceSize t_size, VkDeviceSize t_offset)
{
    return vkMapMemory(device, memory, t_offset, t_size, 0, &mapped);
}

void Buffer::unmap()
{
    if (mapped) {
        vkUnmapMemory(device, memory);
        mapped = nullptr;
    }
}

VkResult Buffer::bind(VkDeviceSize t_offset)
{
    return vkBindBufferMemory(device, buffer, memory, t_offset);
}

void Buffer::setupDescriptor(VkDeviceSize t_size, VkDeviceSize t_offset)
{
    descriptor.offset = t_offset;
    descriptor.buffer = buffer;
    descriptor.range = t_size;
}

void Buffer::copyTo(void* t_data, VkDeviceSize t_size)
{
    assert(mapped);
    memcpy(mapped, t_data, t_size);
}

VkResult Buffer::flush(VkDeviceSize t_size, VkDeviceSize t_offset)
{
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = memory;
    mappedRange.offset = t_offset;
    mappedRange.size = t_size;
    return vkFlushMappedMemoryRanges(device, 1, &mappedRange);
}

VkResult Buffer::invalidate(VkDeviceSize t_size, VkDeviceSize t_offset)
{
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = memory;
    mappedRange.offset = t_offset;
    mappedRange.size = t_size;
    return vkInvalidateMappedMemoryRanges(device, 1, &mappedRange);
}

void Buffer::destroy()
{
    if (buffer) {
        vkDestroyBuffer(device, buffer, nullptr);
    }
    if (memory) {
        vkFreeMemory(device, memory, nullptr);
    }
}

VkDeviceAddress Buffer::getDeviceAddress()
{
    VkBufferDeviceAddressInfo info;
    info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.pNext = nullptr;
    info.buffer = buffer;
    return vkGetBufferDeviceAddressKHR(device, &info);
}
