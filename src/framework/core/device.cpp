/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "device.h"

#include <utility>

Device::Device(VkPhysicalDevice t_physicalDevice)
{
    assert(t_physicalDevice);
    this->physicalDevice = t_physicalDevice;

    // Store Properties features, limits and properties of the physical device for
    // later use Device properties also contain limits and sparse properties
    vkGetPhysicalDeviceProperties(t_physicalDevice, &properties);
    // Features should be checked by the examples before using them
    vkGetPhysicalDeviceFeatures(t_physicalDevice, &features);
    // Memory properties are used regularly for creating all kinds of buffers
    vkGetPhysicalDeviceMemoryProperties(t_physicalDevice, &memoryProperties);
    // Queue family properties, used for setting up requested queues upon device
    // creation
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(t_physicalDevice, &queueFamilyCount, nullptr);
    assert(queueFamilyCount > 0);
    queueFamilyProperties.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(t_physicalDevice,
        &queueFamilyCount,
        queueFamilyProperties.data());

    // Get list of supported extensions
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(t_physicalDevice, nullptr, &extCount, nullptr);
    if (extCount > 0) {
        std::vector<VkExtensionProperties> extensions(extCount);
        if (vkEnumerateDeviceExtensionProperties(t_physicalDevice,
                nullptr,
                &extCount,
                &extensions.front())
            == VK_SUCCESS) {
            for (auto ext : extensions) {
                supportedExtensions.emplace_back(ext.extensionName);
            }
        }
    }
}

Device::~Device()
{
    if (commandPool) {
        vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
    }
    if (logicalDevice) {
        vkDestroyDevice(logicalDevice, nullptr);
    }
}

uint32_t Device::getMemoryType(
    uint32_t t_typeBits, VkMemoryPropertyFlags t_properties, VkBool32* t_memTypeFound) const
{
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((t_typeBits & 1) == 1) {
            if ((memoryProperties.memoryTypes[i].propertyFlags & t_properties) == t_properties) {
                if (t_memTypeFound) {
                    *t_memTypeFound = true;
                }
                return i;
            }
        }
        t_typeBits >>= 1;
    }

    if (t_memTypeFound) {
        *t_memTypeFound = false;
        return 0;
    } else {
        throw std::runtime_error("Could not find a matching memory type");
    }
}

uint32_t Device::getQueueFamilyIndex(VkQueueFlagBits t_queueFlags)
{
    // Dedicated queue for compute
    // Try to find a queue family index that supports compute but not graphics
    if (t_queueFlags & VK_QUEUE_COMPUTE_BIT) {
        for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
            if ((queueFamilyProperties[i].queueFlags & t_queueFlags)
                && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)) {
                return i;
            }
        }
    }

    // Dedicated queue for transfer
    // Try to find a queue family index that supports transfer but not graphics
    // and compute
    if (t_queueFlags & VK_QUEUE_TRANSFER_BIT) {
        for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
            if ((queueFamilyProperties[i].queueFlags & t_queueFlags)
                && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
                && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)) {
                return i;
            }
        }
    }

    // For other queue types or if no separate compute queue is present, return
    // the first one to support the requested flags
    for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
        if (queueFamilyProperties[i].queueFlags & t_queueFlags) {
            return i;
        }
    }

    throw std::runtime_error("Could not find a matching queue family index");
}

VkResult Device::createLogicalDevice(VkPhysicalDeviceFeatures t_enabledFeatures,
    std::vector<const char*> t_enabledExtensions, void* t_pNextChain, bool t_useSwapChain,
    VkQueueFlags t_requestedQueueTypes)
{
    // Desired queues need to be requested upon logical device creation
    // Due to differing queue family configurations of Vulkan implementations this
    // can be a bit tricky, especially if the application requests different queue
    // types

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos {};

    // Get queue family indices for the requested queue family types
    // Note that the indices may overlap depending on the implementation

    const float defaultQueuePriority(0.0f);

    // Graphics queue
    if (t_requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT) {
        queueFamilyIndices.graphics = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
        VkDeviceQueueCreateInfo queueInfo {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = queueFamilyIndices.graphics;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &defaultQueuePriority;
        queueCreateInfos.push_back(queueInfo);
    } else {
        queueFamilyIndices.graphics = VK_NULL_HANDLE;
    }

    // Dedicated compute queue
    if (t_requestedQueueTypes & VK_QUEUE_COMPUTE_BIT) {
        queueFamilyIndices.compute = getQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
        if (queueFamilyIndices.compute != queueFamilyIndices.graphics) {
            // If compute family index differs, we need an additional queue create
            // info for the compute queue
            VkDeviceQueueCreateInfo queueInfo {};
            queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo.queueFamilyIndex = queueFamilyIndices.compute;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &defaultQueuePriority;
            queueCreateInfos.push_back(queueInfo);
        }
    } else {
        // Else we use the same queue
        queueFamilyIndices.compute = queueFamilyIndices.graphics;
    }

    // Dedicated transfer queue
    if (t_requestedQueueTypes & VK_QUEUE_TRANSFER_BIT) {
        queueFamilyIndices.transfer = getQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
        if ((queueFamilyIndices.transfer != queueFamilyIndices.graphics)
            && (queueFamilyIndices.transfer != queueFamilyIndices.compute)) {
            // If compute family index differs, we need an additional queue create
            // info for the compute queue
            VkDeviceQueueCreateInfo queueInfo {};
            queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo.queueFamilyIndex = queueFamilyIndices.transfer;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &defaultQueuePriority;
            queueCreateInfos.push_back(queueInfo);
        }
    } else {
        // Else we use the same queue
        queueFamilyIndices.transfer = queueFamilyIndices.graphics;
    }

    // Create the logical device representation
    std::vector<const char*> deviceExtensions(std::move(t_enabledExtensions));
    if (t_useSwapChain) {
        // If the device will be used for presenting to a display via a swapchain we
        // need to request the swapchain extension
        deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = &t_enabledFeatures;

    // If a pNext(Chain) has been passed, we need to add it to the device creation
    // info
    VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 {};
    if (t_pNextChain) {
        physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        physicalDeviceFeatures2.features = t_enabledFeatures;
        physicalDeviceFeatures2.pNext = t_pNextChain;
        deviceCreateInfo.pEnabledFeatures = nullptr;
        deviceCreateInfo.pNext = &physicalDeviceFeatures2;
    }

    if (!deviceExtensions.empty()) {
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    }

    VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice);

    if (result == VK_SUCCESS) {
        // Create a default command pool for graphics command buffers
        commandPool = createCommandPool(queueFamilyIndices.graphics);
    }

    this->enabledFeatures = t_enabledFeatures;

    return result;
}

VkCommandPool Device::createCommandPool(
    uint32_t t_queueFamilyIndex, VkCommandPoolCreateFlags t_createFlags) const
{
    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = t_queueFamilyIndex;
    cmdPoolInfo.flags = t_createFlags;
    VkCommandPool cmdPool;
    CHECK_RESULT(vkCreateCommandPool(logicalDevice, &cmdPoolInfo, nullptr, &cmdPool))
    return cmdPool;
}

VkCommandBuffer Device::createCommandBuffer(
    VkCommandBufferLevel t_level, VkCommandPool t_pool, bool t_begin) const
{
    VkCommandBufferAllocateInfo cmdBufAllocateInfo {};
    cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufAllocateInfo.commandPool = t_pool;
    cmdBufAllocateInfo.level = t_level;
    cmdBufAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer;
    CHECK_RESULT(vkAllocateCommandBuffers(logicalDevice, &cmdBufAllocateInfo, &cmdBuffer))
    // If requested, also start recording for the new command buffer
    if (t_begin) {
        VkCommandBufferBeginInfo cmdBufInfo = {};
        cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo))
    }
    return cmdBuffer;
}

VkCommandBuffer Device::createCommandBuffer(VkCommandBufferLevel t_level, bool t_begin) const
{
    return createCommandBuffer(t_level, commandPool, t_begin);
}

void Device::flushCommandBuffer(
    VkCommandBuffer t_commandBuffer, VkQueue t_queue, VkCommandPool t_pool, bool t_free) const
{
    if (t_commandBuffer == VK_NULL_HANDLE) {
        return;
    }

    CHECK_RESULT(vkEndCommandBuffer(t_commandBuffer))

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &t_commandBuffer;
    CHECK_RESULT(vkQueueSubmit(t_queue, 1, &submitInfo, VK_NULL_HANDLE))
    CHECK_RESULT(vkQueueWaitIdle(t_queue))
    if (t_free) {
        vkFreeCommandBuffers(logicalDevice, t_pool, 1, &t_commandBuffer);
    }
}

void Device::flushCommandBuffer(
    VkCommandBuffer t_commandBuffer, VkQueue t_queue, bool t_free) const
{
    return flushCommandBuffer(t_commandBuffer, t_queue, commandPool, t_free);
}

bool Device::extensionSupported(const std::string& t_extension)
{
    return (std::find(supportedExtensions.begin(), supportedExtensions.end(), t_extension)
        != supportedExtensions.end());
}

VkFormat Device::getSupportedDepthFormat(bool t_checkSamplingSupport)
{
    // All depth formats may be optional, so we need to find a suitable depth
    // format to use
    std::vector<VkFormat> depthFormats = { VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM };
    for (auto& format : depthFormats) {
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
        // Format must support depth stencil attachment for optimal tiling
        if (formatProperties.optimalTilingFeatures
            & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            if (t_checkSamplingSupport) {
                if (!(formatProperties.optimalTilingFeatures
                        & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                    continue;
                }
            }
            return format;
        }
    }
    throw std::runtime_error("Could not find a matching depth format");
}
