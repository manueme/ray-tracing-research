/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "acceleration_structure.h"

AccelerationStructure::AccelerationStructure() = default;

AccelerationStructure::AccelerationStructure(Device* t_device,
    VkAccelerationStructureTypeKHR t_type, VkAccelerationStructureBuildSizesInfoKHR t_buildSizeInfo)
{
    m_device = t_device->logicalDevice;
    initFunctionPointers();

    VkBufferCreateInfo bufferCreateInfo {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = t_buildSizeInfo.accelerationStructureSize;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    CHECK_RESULT(
        vkCreateBuffer(t_device->logicalDevice, &bufferCreateInfo, nullptr, &m_buffer));
    VkMemoryRequirements memoryRequirements {};
    vkGetBufferMemoryRequirements(t_device->logicalDevice, m_buffer, &memoryRequirements);
    VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo {};
    memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
    VkMemoryAllocateInfo memoryAllocateInfo {};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = t_device->getMemoryType(memoryRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    CHECK_RESULT(
        vkAllocateMemory(t_device->logicalDevice, &memoryAllocateInfo, nullptr, &m_memory));
    CHECK_RESULT(vkBindBufferMemory(t_device->logicalDevice, m_buffer, m_memory, 0));
    // Acceleration structure
    VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo {};
    accelerationStructureCreateInfo.sType
        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    accelerationStructureCreateInfo.buffer = m_buffer;
    accelerationStructureCreateInfo.size = t_buildSizeInfo.accelerationStructureSize;
    accelerationStructureCreateInfo.type = t_type;
    vkCreateAccelerationStructureKHR(t_device->logicalDevice,
        &accelerationStructureCreateInfo,
        nullptr,
        &m_accelerationStructure);
    // AS device address
    VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo {};
    accelerationDeviceAddressInfo.sType
        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    accelerationDeviceAddressInfo.accelerationStructure = m_accelerationStructure;
    m_deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(t_device->logicalDevice,
        &accelerationDeviceAddressInfo);
}

AccelerationStructure::~AccelerationStructure() = default;

VkAccelerationStructureKHR AccelerationStructure::getHandle()
{
    return m_accelerationStructure;
}

uint64_t AccelerationStructure::getDeviceAddress() const {
    return m_deviceAddress;
}

void AccelerationStructure::destroy() {
    vkFreeMemory(m_device, m_memory, nullptr);
    vkDestroyBuffer(m_device, m_buffer, nullptr);
    vkDestroyAccelerationStructureKHR(m_device, m_accelerationStructure, nullptr);
}
