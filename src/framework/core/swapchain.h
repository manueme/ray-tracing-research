/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_SWAPCHAIN_H
#define MANUEME_SWAPCHAIN_H

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../tools/tools.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

using SwapChainBuffer = struct {
    VkImage image;
    VkImageView view;
};

class SwapChain {
private:
    VkInstance m_instance {};
    VkDevice m_device {};
    VkPhysicalDevice m_physicalDevice {};
    VkSurfaceKHR m_surface {};

public:
    SwapChain();
    ~SwapChain();

    VkFormat colorFormat;
    VkColorSpaceKHR colorSpace;

    // Handle to the current swap chain, required for recreation
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;

    uint32_t imageCount {};
    std::vector<VkImage> images;
    std::vector<SwapChainBuffer> buffers;

    // Queue family index of the detected graphics and presenting device queue
    uint32_t queueNodeIndex = UINT32_MAX;

    void initSurface(GLFWwindow* t_window);

    /**
     * Set instance, physical and logical device to use for the swapchain and get
     * all required function pointers
     *
     * @param t_instance Vulkan instance to use
     * @param t_physicalDevice Physical device used to query properties and
     * formats relevant to the swapchain
     * @param device Logical representation of the device to create the swapchain
     * for
     *
     */
    void connect(VkInstance t_instance, VkPhysicalDevice t_physicalDevice, VkDevice device);

    /**
     * Create the swapchain and get its images with given width and height
     *
     * @param t_width Pointer to the width of the swapchain (may be adjusted to
     * fit the requirements of the swapchain)
     * @param t_height Pointer to the height of the swapchain (may be adjusted to
     * fit the requirements of the swapchain)
     * @param t_vsync (Optional) Can be used to force vsync'd rendering (by using
     * VK_PRESENT_MODE_FIFO_KHR as presentation mode)
     */
    void create(uint32_t* t_width, uint32_t* t_height, bool t_vsync = false);

    /**
     * Acquires the next image in the swap chain
     *
     * @param t_presentCompleteSemaphore (Optional) Semaphore that is signaled
     * when the image is ready for use
     * @param t_imageIndex Pointer to the image index that will be increased if
     * the next image could be acquired
     *
     * @note The function will always wait until the next image has been acquired
     * by setting timeout to UINT64_MAX
     *
     * @return VkResult of the image acquisition
     */
    VkResult acquireNextImage(VkSemaphore t_presentCompleteSemaphore, uint32_t* t_imageIndex);

    /**
     * Queue an image for presentation
     *
     * @param t_queue Presentation queue for presenting the image
     * @param t_imageIndex Index of the swapchain image to queue for presentation
     * @param t_waitSemaphore (Optional) Semaphore that is waited on before the
     * image is presented (only used if != VK_NULL_HANDLE)
     *
     * @return VkResult of the queue presentation
     */
    VkResult queuePresent(
        VkQueue t_queue, uint32_t t_imageIndex, VkSemaphore* t_waitSemaphore = nullptr);

    /**
     * Destroy and free Vulkan resources used for the swapchain
     */
    void cleanup();
};

#endif // MANUEME_SWAPCHAIN_H
