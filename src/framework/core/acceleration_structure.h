/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_ACCELERATION_STRUCTURE_H
#define MANUEME_ACCELERATION_STRUCTURE_H

#include "../base_project.h"

// This represents one BLAS with multiple geometries one per "mesh".
// NOTE: The instance definition of this implementation contains only one material index
// if you want to add multiple meshes to a BLAS they must have all the same material. Feel free to
// create a per-primitive material implementation to support multiple materials per BLAS.
struct BlasCreateInfo {
    std::vector<VkAccelerationStructureGeometryKHR> geomery;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> meshes; // could also be called "offsets"
};

struct TlasCreateInfo {
    std::vector<VkAccelerationStructureInstanceKHR> instances;
    bool update = false;
};

class AccelerationStructure {
public:
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    void initFunctionPointers()
    {
        vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
            vkGetDeviceProcAddr(m_device, "vkCreateAccelerationStructureKHR"));
        vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
            vkGetDeviceProcAddr(m_device, "vkDestroyAccelerationStructureKHR"));
        vkGetAccelerationStructureDeviceAddressKHR
            = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
                vkGetDeviceProcAddr(m_device, "vkGetAccelerationStructureDeviceAddressKHR"));
    }

    AccelerationStructure();
    AccelerationStructure(Device* t_device, VkAccelerationStructureTypeKHR t_type,
        VkAccelerationStructureBuildSizesInfoKHR t_buildSizeInfo);
    ~AccelerationStructure();

    void destroy();

    VkAccelerationStructureKHR getHandle();

    uint64_t getDeviceAddress() const;

private:
    VkDevice m_device;
    VkAccelerationStructureKHR m_accelerationStructure;
    uint64_t m_deviceAddress = 0;
    VkDeviceMemory m_memory;
    VkBuffer m_buffer;
};

#endif // MANUEME_ACCELERATION_STRUCTURE_H
