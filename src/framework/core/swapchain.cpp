/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */
#include "swapchain.h"

SwapChain::SwapChain() = default;
SwapChain::~SwapChain() = default;

void SwapChain::initSurface(GLFWwindow* t_window)
{
    VKM_CHECK_RESULT(glfwCreateWindowSurface(m_instance, t_window, nullptr, &m_surface))
    // Get available queue family properties
    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueCount, nullptr);
    assert(queueCount >= 1);

    std::vector<VkQueueFamilyProperties> queueProps(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueCount, queueProps.data());

    // Iterate over each queue to learn whether it supports presenting:
    // Find a queue with present support
    // Will be used to present the swap chain images to the windowing system
    std::vector<VkBool32> supportsPresent(queueCount);
    for (uint32_t i = 0; i < queueCount; i++) {
        vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &supportsPresent[i]);
    }

    // Search for a graphics and a present queue in the array of queue
    // families, try to find one that supports both
    uint32_t graphicsQueueNodeIndex = UINT32_MAX;
    uint32_t presentQueueNodeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queueCount; i++) {
        if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            if (graphicsQueueNodeIndex == UINT32_MAX) {
                graphicsQueueNodeIndex = i;
            }

            if (supportsPresent[i] == VK_TRUE) {
                graphicsQueueNodeIndex = i;
                presentQueueNodeIndex = i;
                break;
            }
        }
    }
    if (presentQueueNodeIndex == UINT32_MAX) {
        // If there's no queue that supports both present and graphics
        // try to find a separate present queue
        for (uint32_t i = 0; i < queueCount; ++i) {
            if (supportsPresent[i] == VK_TRUE) {
                presentQueueNodeIndex = i;
                break;
            }
        }
    }

    // Exit if either a graphics or a presenting queue hasn't been found
    if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX) {
        throw std::logic_error("Could not find a graphics and/or presenting queue!");
    }

    if (graphicsQueueNodeIndex != presentQueueNodeIndex) {
        throw std::logic_error("Separate graphics and presenting queues are not supported yet!");
    }

    queueNodeIndex = graphicsQueueNodeIndex;

    // Get list of supported surface formats
    uint32_t formatCount = 0;
    VKM_CHECK_RESULT(
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr))
    assert(formatCount > 0);

    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    VKM_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice,
        m_surface,
        &formatCount,
        surfaceFormats.data()))

    // If the surface format list only includes one entry with
    // VK_FORMAT_UNDEFINED, there is no preferered format, so we assume
    // VK_FORMAT_B8G8R8A8_UNORM
    if ((formatCount == 1) && (surfaceFormats[0].format == VK_FORMAT_UNDEFINED)) {
        colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
        colorSpace = surfaceFormats[0].colorSpace;
    } else {
        // iterate over the list of available surface format and
        // check for the presence of VK_FORMAT_B8G8R8A8_UNORM
        bool found_B8G8R8A8_UNORM = false;
        for (auto&& surfaceFormat : surfaceFormats) {
            if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM) {
                colorFormat = surfaceFormat.format;
                colorSpace = surfaceFormat.colorSpace;
                found_B8G8R8A8_UNORM = true;
                break;
            }
        }

        // in case VK_FORMAT_B8G8R8A8_UNORM is not available
        // select the first available color format
        if (!found_B8G8R8A8_UNORM) {
            colorFormat = surfaceFormats[0].format;
            colorSpace = surfaceFormats[0].colorSpace;
        }
    }
}

void SwapChain::connect(
    VkInstance t_instance, VkPhysicalDevice t_physicalDevice, VkDevice device)
{
    this->m_instance = t_instance;
    this->m_physicalDevice = t_physicalDevice;
    this->m_device = device;
}

void SwapChain::create(uint32_t* t_width, uint32_t* t_height, bool t_vsync)
{
    VkSwapchainKHR oldSwapChain = swapChain;

    // Get physical device surface properties and formats
    VkSurfaceCapabilitiesKHR surfCaps;
    VKM_CHECK_RESULT(
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &surfCaps))

    // Get available present modes
    uint32_t presentModeCount = 0;
    VKM_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice,
        m_surface,
        &presentModeCount,
        nullptr))
    assert(presentModeCount > 0);

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    VKM_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice,
        m_surface,
        &presentModeCount,
        presentModes.data()))

    VkExtent2D swapChainExtent = {};
    // If width (and height) equals the special value 0xFFFFFFFF, the size of the
    // surface will be set by the swap chain
    if (surfCaps.currentExtent.width == static_cast<uint32_t>(-1)) {
        // If the surface size is undefined, the size is set to
        // the size of the images requested.
        swapChainExtent.width = *t_width;
        swapChainExtent.height = *t_height;
    } else {
        // If the surface size is defined, the swap chain size must match
        swapChainExtent = surfCaps.currentExtent;
        *t_width = surfCaps.currentExtent.width;
        *t_height = surfCaps.currentExtent.height;
    }

    // Select a present mode for the swap chain

    // The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
    // This mode waits for the vertical blank ("v-sync")
    VkPresentModeKHR swapChainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

    // If v-sync is not requested, try to find a mailbox mode
    // It's the lowest latency non-tearing present mode available
    if (!t_vsync) {
        for (size_t i = 0; i < presentModeCount; i++) {
            if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                swapChainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            } else if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                swapChainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
        }
    }

    // Determine the number of images
    uint32_t desiredNumberOfSwapchainImages = surfCaps.minImageCount + 1;
    if ((surfCaps.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfCaps.maxImageCount)) {
        desiredNumberOfSwapchainImages = surfCaps.maxImageCount;
    }

    // Find the transformation of the surface
    VkSurfaceTransformFlagsKHR preTransform;
    if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        // We prefer a non-rotated transform
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        preTransform = surfCaps.currentTransform;
    }

    // Find a supported composite alpha format (not all devices support alpha
    // opaque)
    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    // Simply select the first composite alpha format available
    std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (auto& compositeAlphaFlag : compositeAlphaFlags) {
        if (surfCaps.supportedCompositeAlpha & compositeAlphaFlag) {
            compositeAlpha = compositeAlphaFlag;
            break;
        }
    }

    VkSwapchainCreateInfoKHR swapChainCI = {};
    swapChainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCI.pNext = nullptr;
    swapChainCI.surface = m_surface;
    swapChainCI.minImageCount = desiredNumberOfSwapchainImages;
    swapChainCI.imageFormat = colorFormat;
    swapChainCI.imageColorSpace = colorSpace;
    swapChainCI.imageExtent = { swapChainExtent.width, swapChainExtent.height };
    swapChainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapChainCI.preTransform = static_cast<VkSurfaceTransformFlagBitsKHR>(preTransform);
    swapChainCI.imageArrayLayers = 1;
    swapChainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapChainCI.queueFamilyIndexCount = 0;
    swapChainCI.pQueueFamilyIndices = nullptr;
    swapChainCI.presentMode = swapChainPresentMode;
    swapChainCI.oldSwapchain = oldSwapChain;
    // Setting clipped to VK_TRUE allows the implementation to discard rendering
    // outside of the surface area
    swapChainCI.clipped = VK_TRUE;
    swapChainCI.compositeAlpha = compositeAlpha;

    // Enable transfer source on swap chain images if supported
    if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
        swapChainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    // Enable transfer destination on swap chain images if supported
    if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        swapChainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    VKM_CHECK_RESULT(vkCreateSwapchainKHR(m_device, &swapChainCI, nullptr, &swapChain))

    // If an existing swap chain is re-created, destroy the old swap chain
    // This also cleans up all the presentable images
    if (oldSwapChain != VK_NULL_HANDLE) {
        for (uint32_t i = 0; i < imageCount; i++) {
            vkDestroyImageView(m_device, buffers[i].view, nullptr);
        }
        vkDestroySwapchainKHR(m_device, oldSwapChain, nullptr);
    }
    VKM_CHECK_RESULT(vkGetSwapchainImagesKHR(m_device, swapChain, &imageCount, nullptr))

    // Get the swap chain images
    images.resize(imageCount);
    VKM_CHECK_RESULT(vkGetSwapchainImagesKHR(m_device, swapChain, &imageCount, images.data()))

    // Get the swap chain buffers containing the image and imageview
    buffers.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo colorAttachmentView = {};
        colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorAttachmentView.pNext = nullptr;
        colorAttachmentView.format = colorFormat;
        colorAttachmentView.components = { VK_COMPONENT_SWIZZLE_R,
            VK_COMPONENT_SWIZZLE_G,
            VK_COMPONENT_SWIZZLE_B,
            VK_COMPONENT_SWIZZLE_A };
        colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorAttachmentView.subresourceRange.baseMipLevel = 0;
        colorAttachmentView.subresourceRange.levelCount = 1;
        colorAttachmentView.subresourceRange.baseArrayLayer = 0;
        colorAttachmentView.subresourceRange.layerCount = 1;
        colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorAttachmentView.flags = 0;

        buffers[i].image = images[i];

        colorAttachmentView.image = buffers[i].image;

        VKM_CHECK_RESULT(
            vkCreateImageView(m_device, &colorAttachmentView, nullptr, &buffers[i].view))
    }
}

VkResult SwapChain::acquireNextImage(
    VkSemaphore t_presentCompleteSemaphore, uint32_t* t_imageIndex)
{
    // By setting timeout to UINT64_MAX we will always wait until the next image
    // has been acquired or an actual error is thrown With that we don't have to
    // handle VK_NOT_READY
    return vkAcquireNextImageKHR(m_device,
        swapChain,
        UINT64_MAX,
        t_presentCompleteSemaphore,
        VK_NULL_HANDLE,
        t_imageIndex);
}

VkResult SwapChain::queuePresent(
    VkQueue t_queue, uint32_t t_imageIndex, VkSemaphore* t_waitSemaphore)
{
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapChain;
    presentInfo.pImageIndices = &t_imageIndex;
    // Check if a wait semaphore has been specified to wait for before presenting
    // the image
    if (t_waitSemaphore) {
        presentInfo.pWaitSemaphores = t_waitSemaphore;
        presentInfo.waitSemaphoreCount = 1;
    }
    return vkQueuePresentKHR(t_queue, &presentInfo);
}

void SwapChain::cleanup()
{
    if (swapChain != VK_NULL_HANDLE) {
        for (uint32_t i = 0; i < imageCount; i++) {
            vkDestroyImageView(m_device, buffers[i].view, nullptr);
        }
    }
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, swapChain, nullptr);
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    }
    m_surface = VK_NULL_HANDLE;
    swapChain = VK_NULL_HANDLE;
}
