/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_TOOLS_H
#define MANUEME_TOOLS_H

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "initializers.hpp"
#include "vulkan/vulkan.h"

// Custom define for better code readability
#define VK_FLAGS_NONE 0
// Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

#define CHECK_RESULT(f)                                                                        \
    {                                                                                              \
        VkResult res = (f);                                                                        \
        if (res != VK_SUCCESS) {                                                                   \
            std::cout << "Fatal : VkResult is \"" << tools::errorString(res) << "\" in "      \
                      << static_cast<const char*>(__FILE__) << " at line "                         \
                      << static_cast<int>(__LINE__) << " in function \""                           \
                      << static_cast<const char*>(__FUNCTION__) << "\"" << std::endl;              \
            assert(res == VK_SUCCESS);                                                             \
        }                                                                                          \
    }

namespace tools {
/** @brief Returns an error code as a string */
std::string errorString(VkResult t_errorCode);

/**
@brief Selects a suitable supported depth format starting with 32 bit down to 16
bit.
@return False if none of the depth formats in the list is supported by the
device **/
VkBool32 getSupportedDepthFormat(VkPhysicalDevice t_physicalDevice, VkFormat* t_depthFormat);

/** @brief Put an image memory barrier for setting an image layout on the sub
 * resource into the given command buffer **/
void setImageLayout(VkCommandBuffer t_cmdbuffer, VkImage t_image, VkImageLayout t_oldImageLayout,
    VkImageLayout t_newImageLayout, VkImageSubresourceRange t_subresourceRange,
    VkPipelineStageFlags t_srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VkPipelineStageFlags t_dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

/** @brief Uses a fixed sub resource layout with first mip level and layer **/
void setImageLayout(VkCommandBuffer t_cmdbuffer, VkImage t_image, VkImageAspectFlags t_aspectMask,
    VkImageLayout t_oldImageLayout, VkImageLayout t_newImageLayout,
    VkPipelineStageFlags t_srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VkPipelineStageFlags t_dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

/** @brief Load a SPIR-V shader (binary) **/
VkShaderModule loadShader(const char* t_fileName, VkDevice t_device);

/** @brief Check for device features */
bool hasRequiredFeatures(
    VkPhysicalDevice t_physicalDevice, VkPhysicalDeviceFeatures t_requiredFeatures);

/** @brief Checks suitability of the device */
bool isDeviceSuitable(VkPhysicalDevice t_physicalDevice,
    const std::vector<const char*>& t_requiredExtensions,
    const VkPhysicalDeviceFeatures& t_requiredFeatures);

/** @brief Calculates a suitability score for a device */
int rateDeviceSuitability(VkPhysicalDevice t_physicalDevice,
    const std::vector<const char*>& t_requiredExtensions,
    const VkPhysicalDeviceFeatures& t_requiredFeatures);

/** @brief Gets the best suitable device */
VkPhysicalDevice getBestSuitableDevice(const std::vector<VkPhysicalDevice>& t_devices,
    const std::vector<const char*>& t_requiredExtensions,
    const VkPhysicalDeviceFeatures& t_requiredFeatures);

void checkDeviceExtensionSupport(
    VkPhysicalDevice t_device, const std::vector<const char*>& t_extensionList);

bool validateDeviceExtensionSupport(
    VkPhysicalDevice t_device, const std::vector<const char*>& t_extensionList);

uint32_t alignedSize(uint32_t t_value, uint32_t t_alignment);

} // namespace tools

#endif // MANUEME_TOOLS_H
