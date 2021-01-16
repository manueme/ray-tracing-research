/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_DEVICE_H
#define MANUEME_DEVICE_H

#include <algorithm>
#include <cassert>
#include <exception>

#include "../tools/tools.h"
#include "buffer.h"
#include "vulkan/vulkan.h"

class Device {
public:
    /**
     * Default constructor
     *
     * @param t_physicalDevice Physical device that is to be used
     */
    explicit Device(VkPhysicalDevice t_physicalDevice);

    /**
     * Default destructor
     *
     * @note Frees the logical device
     */
    ~Device();

    // Physical device representation
    VkPhysicalDevice physicalDevice;

    /// Logical device representation (application's view of the device)
    VkDevice logicalDevice;

    // @brief Properties of the physical device including limits that the application can check against
    VkPhysicalDeviceProperties properties;

    // @brief Features of the physical device that an application can use to check if a feature is supported
    VkPhysicalDeviceFeatures features;

    // Features that have been enabled for use on the physical device
    VkPhysicalDeviceFeatures enabledFeatures;

    // Memory types and heaps of the physical device
    VkPhysicalDeviceMemoryProperties memoryProperties;

    // Queue family properties of the physical device
    std::vector<VkQueueFamilyProperties> queueFamilyProperties;

    // List of extensions supported by the device
    std::vector<std::string> supportedExtensions;

    // Default command pool for the graphics queue family index
    VkCommandPool commandPool = VK_NULL_HANDLE;

    // Contains queue family indices
    struct {
        uint32_t graphics;
        uint32_t compute;
        uint32_t transfer;
    } queueFamilyIndices;

    // Typecast to VkDevice
    explicit operator VkDevice() const { return logicalDevice; };

    /**
     * Get the index of a memory type that has all the requested property bits set
     *
     * @param typeBits Bitmask with bits set for each memory type supported by the
     * resource to request for (from VkMemoryRequirements)
     * @param properties Bitmask of properties for the memory type to request
     * @param (Optional) memTypeFound Pointer to a bool that is set to true if a
     * matching memory type has been found
     *
     * @return Index of the requested memory type
     *
     * @throw Throws an exception if memTypeFound is null and no memory type could
     * be found that supports the requested properties
     */
    uint32_t getMemoryType(uint32_t t_typeBits, VkMemoryPropertyFlags t_properties,
        VkBool32* t_memTypeFound = nullptr) const;

    /**
     * Get the index of a queue family that supports the requested queue flags
     *
     * @param t_queueFlags Queue flags to find a queue family index for
     *
     * @return Index of the queue family index that matches the flags
     *
     * @throw Throws an exception if no queue family index could be found that
     * supports the requested flags
     */
    uint32_t getQueueFamilyIndex(VkQueueFlagBits t_queueFlags);

    /**
     * Create the logical device based on the assigned physical device, also gets
     * default queue family indices
     *
     * @param t_enabledFeatures Can be used to enable certain features upon device
     * creation
     * @param t_pNextChain Optional chain of pointer to extension structures
     * @param t_useSwapChain Set to false for headless rendering to omit the
     * swapchain device extensions
     * @param t_requestedQueueTypes Bit flags specifying the queue types to be
     * requested from the device
     *
     * @return VkResult of the device creation call
     */
    VkResult createLogicalDevice(VkPhysicalDeviceFeatures t_enabledFeatures,
        std::vector<const char*> t_enabledExtensions, void* t_pNextChain,
        bool t_useSwapChain = true,
        VkQueueFlags t_requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

    /**
     * Create a command pool for allocation command buffers from
     *
     * @param t_queueFamilyIndex Family index of the queue to create the command
     * pool for
     * @param t_createFlags (Optional) Command pool creation flags (Defaults to
     * VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
     *
     * @note Command buffers allocated from the created pool can only be submitted
     * to a queue with the same family index
     *
     * @return A handle to the created command buffer
     */
    VkCommandPool createCommandPool(uint32_t t_queueFamilyIndex,
        VkCommandPoolCreateFlags t_createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT) const;

    /**
     * Allocate a command buffer from the command pool
     *
     * @param level Level of the new command buffer (primary or secondary)
     * @param pool Command pool from which the command buffer will be allocated
     * @param (Optional) begin If true, recording on the new command buffer will
     * be started (vkBeginCommandBuffer) (Defaults to false)
     *
     * @return A handle to the allocated command buffer
     */
    VkCommandBuffer createCommandBuffer(
        VkCommandBufferLevel t_level, VkCommandPool t_pool, bool t_begin = false) const;

    VkCommandBuffer createCommandBuffer(VkCommandBufferLevel t_level, bool t_begin = false) const;

    /**
     * Finish command buffer recording and submit it to a queue
     *
     * @param t_commandBuffer Command buffer to flush
     * @param t_queue Queue to submit the command buffer to
     * @param t_pool Command pool on which the command buffer has been created
     * @param t_free (Optional) Free the command buffer once it has been submitted
     * (Defaults to true)
     *
     * @note The queue that the command buffer is submitted to must be from the
     * same family index as the pool it was allocated from
     * @note Uses a fence to ensure command buffer has finished executing
     */
    void flushCommandBuffer(
        VkCommandBuffer t_commandBuffer, VkQueue t_queue, VkCommandPool t_pool, bool t_free = true) const;

    void flushCommandBuffer(VkCommandBuffer t_commandBuffer, VkQueue t_queue, bool t_free = true) const;

    /**
     * Check if an extension is supported by the (physical device)
     *
     * @param t_extension Name of the extension to check
     *
     * @return True if the extension is supported (present in the list read at
     * device creation time)
     */
    bool extensionSupported(const std::string& t_extension);

    /**
     * Select the best-fit depth format for this device from a list of possible
     * depth (and stencil) formats
     *
     * @param t_checkSamplingSupport Check if the format can be sampled from (e.g.
     * for shader reads)
     *
     * @return The depth format that best fits for the current device
     *
     * @throw Throws an exception if no depth format fits the requirements
     */
    VkFormat getSupportedDepthFormat(bool t_checkSamplingSupport);
};

#endif // MANUEME_DEVICE_H
