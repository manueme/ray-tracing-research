/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "base_project.h"

#include "scene/scene.h"
#include "tools/debug.h"
#include <cmath>
#include <string>
#include <utility>

VkResult BaseProject::createInstance(bool t_enableValidation)
{
    this->m_settings.validation = t_enableValidation;

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = m_appName.c_str();
    appInfo.pEngineName = m_appName.c_str();
    appInfo.apiVersion = api_version;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = nullptr;
    glfwExtensions
        = glfwGetRequiredInstanceExtensions(&glfwExtensionCount); // Includes VK_KHR_surface
    std::vector<const char*> instanceExtensions(glfwExtensions,
        glfwExtensions + glfwExtensionCount);
    if (!m_enabledInstanceExtensions.empty()) {
        for (auto enabledExtension : m_enabledInstanceExtensions) {
            instanceExtensions.push_back(enabledExtension);
        }
    }
    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = nullptr;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    if (!instanceExtensions.empty()) {
        if (m_settings.validation) {
            instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
        instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
    }
    if (m_settings.validation) {
        const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
        // Check if this layer is available at instance level
        uint32_t instanceLayerCount = 0;
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
        std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());
        bool validationLayerPresent = false;
        for (VkLayerProperties layer : instanceLayerProperties) {
            if (strcmp(layer.layerName, validationLayerName) == 0) {
                validationLayerPresent = true;
                break;
            }
        }
        if (validationLayerPresent) {
            instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
            instanceCreateInfo.enabledLayerCount = 1;
        } else {
            throw std::runtime_error("Validation layer VK_LAYER_KHRONOS_validation not present");
        }
    }
    return vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
}

size_t BaseProject::getAcquisitionFrameIndex(uint32_t imageIndex) const
{
    size_t frameIndex = m_imageToFrameIndex[imageIndex];
    // Safety check: if mapping is invalid, use imageIndex as fallback
    if (frameIndex == SIZE_MAX) {
        return imageIndex;
    }
    return frameIndex;
}

uint32_t BaseProject::acquireNextImage()
{
    // Before reusing the acquisition semaphore, wait for its fence to ensure it has been consumed
    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    // Acquire the next image from the swap chain
    uint32_t imageIndex = 0;
    VkResult result
        = m_swapChain.acquireNextImage(m_imageAvailableSemaphores[m_currentFrame], &imageIndex);

    // Recreate the SwapChain if it's no longer compatible with the surface
    if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR)) {
        handleWindowResize();
        return UINT32_MAX; // Skip this frame after resize
    }

    CHECK_RESULT(result)

    // Validate imageIndex is within bounds
    if (imageIndex >= m_swapChain.imageCount) {
        return UINT32_MAX; // Invalid index, skip this frame
    }

    // Wait for the fence of this image to ensure it's not being used by a previous frame
    vkWaitForFences(m_device, 1, &m_inFlightFences[imageIndex], VK_TRUE, UINT64_MAX);

    // Store which frame index was used for acquisition (needed for submit)
    m_imageToFrameIndex[imageIndex] = m_currentFrame;

    return imageIndex;
};

VkResult BaseProject::queuePresentSwapChain(uint32_t t_imageIndex)
{
    // Use imageIndex for the semaphore that was signaled in the compute submit
    auto result = m_swapChain.queuePresent(m_queue,
        t_imageIndex,
        &m_renderFinishedSemaphores[t_imageIndex]);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
        m_framebufferResized = false;
        handleWindowResize();
        // After resize, m_currentFrame was reset to 0, so don't rotate it
        // The next acquireNextImage will use m_currentFrame = 0
    } else if (result != VK_SUCCESS) {
        CHECK_RESULT(result)
    } else {
        // Only rotate frame index if present was successful and no resize occurred
        m_currentFrame = (m_currentFrame + 1) % m_swapChain.imageCount;
    }

    return result;
}

void BaseProject::createCommandBuffers()
{
    // Create one command buffer for each swap chain image and reuse for rendering
    m_drawCmdBuffers.resize(m_swapChain.imageCount);

    VkCommandBufferAllocateInfo cmdBufAllocateInfo {};
    cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufAllocateInfo.commandPool = m_cmdPool;
    cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufAllocateInfo.commandBufferCount = static_cast<uint32_t>(m_drawCmdBuffers.size());

    CHECK_RESULT(vkAllocateCommandBuffers(m_device, &cmdBufAllocateInfo, m_drawCmdBuffers.data()))
}

void BaseProject::destroyCommandBuffers()
{
    vkFreeCommandBuffers(m_device,
        m_cmdPool,
        static_cast<uint32_t>(m_drawCmdBuffers.size()),
        m_drawCmdBuffers.data());
}

void BaseProject::createPipelineCache()
{
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    CHECK_RESULT(
        vkCreatePipelineCache(m_device, &pipelineCacheCreateInfo, nullptr, &m_pipelineCache))
}

void BaseProject::destroyComputeCommandBuffers()
{
    vkFreeCommandBuffers(m_device,
        m_compute.commandPool,
        static_cast<uint32_t>(m_compute.commandBuffers.size()),
        m_compute.commandBuffers.data());
}

void BaseProject::createComputeCommandBuffers()
{
    m_compute.commandBuffers.resize(m_swapChain.imageCount);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = m_compute.commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount
        = static_cast<uint32_t>(m_compute.commandBuffers.size());
    CHECK_RESULT(vkAllocateCommandBuffers(m_device,
        &commandBufferAllocateInfo,
        m_compute.commandBuffers.data()))
}

void BaseProject::prepareCompute()
{
    vkGetDeviceQueue(m_device, m_vulkanDevice->queueFamilyIndices.compute, 0, &m_compute.queue);

    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = m_vulkanDevice->queueFamilyIndices.compute;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    CHECK_RESULT(vkCreateCommandPool(m_device, &cmdPoolInfo, nullptr, &m_compute.commandPool))

    createComputeCommandBuffers();

    VkFenceCreateInfo fenceCreateInfo {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    m_compute.fences.resize(m_swapChain.imageCount);
    m_compute.semaphores.resize(m_swapChain.imageCount);
    for (size_t i = 0; i < m_swapChain.imageCount; ++i) {
        CHECK_RESULT(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_compute.fences[i]))
        std::string name = "ComputeFence[" + std::to_string(i) + "]";
        debug::setObjectName(m_device,
            (uint64_t)m_compute.fences[i],
            VK_OBJECT_TYPE_FENCE,
            name.c_str());

        CHECK_RESULT(
            vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_compute.semaphores[i]))
        name = "ComputeSemaphore[" + std::to_string(i) + "]";
        debug::setObjectName(m_device,
            (uint64_t)m_compute.semaphores[i],
            VK_OBJECT_TYPE_SEMAPHORE,
            name.c_str());
    }
}

void BaseProject::prepare()
{
    initSwapChain();
    createCommandPool();
    setupSwapChain();
    createCommandBuffers();
    createSynchronizationPrimitives();
    setupDepthStencil();
    setupRenderPass();
    createPipelineCache();
    setupFrameBuffer();

    if (m_settings.useCompute) {
        prepareCompute();
    }
}

VkPipelineShaderStageCreateInfo BaseProject::loadShader(const std::string& t_fileName,
    VkShaderStageFlagBits t_stage)
{
    VkPipelineShaderStageCreateInfo shaderStage = {};
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage = t_stage;
    shaderStage.module = tools::loadShader(t_fileName.c_str(), m_device);
    shaderStage.pName = "main";
    assert(shaderStage.module != VK_NULL_HANDLE);
    m_shaderModules.push_back(shaderStage.module);
    return shaderStage;
}

void BaseProject::nextFrame()
{
    auto timeStart = std::chrono::high_resolution_clock::now();
    if (m_viewUpdated) {
        m_viewUpdated = false;
        viewChanged();
    }

    const auto millisecond = 1000.0f;

    render();
    ++m_frameCounter;
    auto timeEnd = std::chrono::high_resolution_clock::now();
    auto timeDiff = std::chrono::duration<double, std::milli>(timeEnd - timeStart).count();
    m_frameTimer = static_cast<float>(timeDiff) / millisecond;
    auto camera = m_scene->getCamera();
    camera->update(m_frameTimer);
    if (camera->moving()) {
        m_viewUpdated = true;
    }

    float fpsTimer = static_cast<float>(
        std::chrono::duration<double, std::milli>(timeEnd - m_lastTimestamp).count());
    if (fpsTimer > millisecond) {
        m_lastFps
            = static_cast<uint32_t>(static_cast<float>(m_frameCounter) * (millisecond / fpsTimer));
        m_frameCounter = 0;
        m_lastTimestamp = timeEnd;
    }
}

BaseProject::BaseProject(std::string t_appName, std::string t_windowTitle, bool t_enableValidation)
    : m_appName(std::move(t_appName))
    , m_windowTitle(std::move(t_windowTitle))
{
    m_settings.validation = t_enableValidation;
}

void BaseProject::saveScreenshot(const char* filename)
{
    bool supportsBlit = true;

    // Check blit support for source and destination
    VkFormatProperties formatProps;

    // Check if the device supports blitting from optimal images (the swapchain images are in
    // optimal format)
    vkGetPhysicalDeviceFormatProperties(m_vulkanDevice->physicalDevice,
        m_swapChain.colorFormat,
        &formatProps);
    if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
        std::cerr << "Device does not support blitting from optimal tiled images, using copy "
                     "instead of blit!"
                  << std::endl;
        supportsBlit = false;
    }

    // Check if the device supports blitting to linear images
    vkGetPhysicalDeviceFormatProperties(m_vulkanDevice->physicalDevice,
        VK_FORMAT_R8G8B8A8_UNORM,
        &formatProps);
    if (!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
        std::cerr << "Device does not support blitting to linear tiled images, using copy instead "
                     "of blit!"
                  << std::endl;
        supportsBlit = false;
    }

    // Source for the copy is the last rendered swapchain image
    VkImage srcImage = m_swapChain.images[m_currentFrame];

    // Create the linear tiled destination image to copy to and to read the memory from
    VkImageCreateInfo imageCreateCI = {};
    imageCreateCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateCI.imageType = VK_IMAGE_TYPE_2D;
    // Note that vkCmdBlitImage (if supported) will also do format conversions if the swapchain
    // color format would differ
    imageCreateCI.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageCreateCI.extent.width = m_width;
    imageCreateCI.extent.height = m_height;
    imageCreateCI.extent.depth = 1;
    imageCreateCI.arrayLayers = 1;
    imageCreateCI.mipLevels = 1;
    imageCreateCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateCI.tiling = VK_IMAGE_TILING_LINEAR;
    imageCreateCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    // Create the image
    VkImage dstImage = nullptr;
    CHECK_RESULT(vkCreateImage(m_device, &imageCreateCI, nullptr, &dstImage));
    // Create memory to back up the image
    VkMemoryRequirements memRequirements;
    VkMemoryAllocateInfo memAllocInfo {};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VkDeviceMemory dstImageMemory;
    vkGetImageMemoryRequirements(m_device, dstImage, &memRequirements);
    memAllocInfo.allocationSize = memRequirements.size;
    // Memory must be host visible to copy from
    memAllocInfo.memoryTypeIndex = m_vulkanDevice->getMemoryType(memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    CHECK_RESULT(vkAllocateMemory(m_device, &memAllocInfo, nullptr, &dstImageMemory));
    CHECK_RESULT(vkBindImageMemory(m_device, dstImage, dstImageMemory, 0));

    // Do the actual blit from the swapchain image to our host visible destination image
    VkCommandBuffer copyCmd
        = m_vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // Transition destination image to transfer destination layout
    tools::insertImageMemoryBarrier(copyCmd,
        dstImage,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    // Transition swapchain image from present to transfer source layout
    tools::insertImageMemoryBarrier(copyCmd,
        srcImage,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    // If source and destination support blit we'll blit as this also does automatic format
    // conversion (e.g. from BGR to RGB)
    if (supportsBlit) {
        // Define the region to blit (we will blit the whole swapchain image)
        VkOffset3D blitSize;
        blitSize.x = m_width;
        blitSize.y = m_height;
        blitSize.z = 1;
        VkImageBlit imageBlitRegion {};
        imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlitRegion.srcSubresource.layerCount = 1;
        imageBlitRegion.srcOffsets[1] = blitSize;
        imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlitRegion.dstSubresource.layerCount = 1;
        imageBlitRegion.dstOffsets[1] = blitSize;

        // Issue the blit command
        vkCmdBlitImage(copyCmd,
            srcImage,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dstImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &imageBlitRegion,
            VK_FILTER_NEAREST);
    } else {
        // Otherwise use image copy (requires us to manually flip components)
        VkImageCopy imageCopyRegion {};
        imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopyRegion.srcSubresource.layerCount = 1;
        imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopyRegion.dstSubresource.layerCount = 1;
        imageCopyRegion.extent.width = m_width;
        imageCopyRegion.extent.height = m_height;
        imageCopyRegion.extent.depth = 1;

        // Issue the copy command
        vkCmdCopyImage(copyCmd,
            srcImage,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dstImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &imageCopyRegion);
    }

    // Transition destination image to general layout, which is the required layout for mapping the
    // image memory later on
    tools::insertImageMemoryBarrier(copyCmd,
        dstImage,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    // Transition back the swap chain image after the blit is done
    tools::insertImageMemoryBarrier(copyCmd,
        srcImage,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    m_vulkanDevice->flushCommandBuffer(copyCmd, m_queue);

    // Get layout of the image (including row pitch)
    VkImageSubresource subResource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    VkSubresourceLayout subResourceLayout;
    vkGetImageSubresourceLayout(m_device, dstImage, &subResource, &subResourceLayout);

    // Map image memory so we can start copying from it
    const char* data = nullptr;
    vkMapMemory(m_device, dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void**)&data);
    data += subResourceLayout.offset;

    std::ofstream file(filename, std::ios::out | std::ios::binary);

    // ppm header
    file << "P6\n" << m_width << "\n" << m_height << "\n" << 255 << "\n";

    // If source is BGR (destination is always RGB) and we can't use blit (which does automatic
    // conversion), we'll have to manually swizzle color components
    bool colorSwizzle = false;
    // Check if source is BGR
    // Note: Not complete, only contains most common and basic BGR surface formats for demonstration
    // purposes
    if (!supportsBlit) {
        std::vector<VkFormat> formatsBGR
            = { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SNORM };
        colorSwizzle = (std::find(formatsBGR.begin(), formatsBGR.end(), m_swapChain.colorFormat)
            != formatsBGR.end());
    }

    // ppm binary pixel data
    for (uint32_t y = 0; y < m_height; y++) {
        auto* row = (unsigned int*)data;
        for (uint32_t x = 0; x < m_width; x++) {
            if (colorSwizzle) {
                file.write(reinterpret_cast<char*>(row) + 2, 1);
                file.write(reinterpret_cast<char*>(row) + 1, 1);
                file.write(reinterpret_cast<char*>(row), 1);
            } else {
                file.write(reinterpret_cast<char*>(row), 3);
            }
            row++;
        }
        data += subResourceLayout.rowPitch;
    }
    file.close();

    std::cout << "Screenshot saved to disk" << std::endl;

    // Clean up resources
    vkUnmapMemory(m_device, dstImageMemory);
    vkFreeMemory(m_device, dstImageMemory, nullptr);
    vkDestroyImage(m_device, dstImage, nullptr);
}

BaseProject::~BaseProject()
{
    // Clean up Vulkan resources
    m_swapChain.cleanup();
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    }
    destroyCommandBuffers();
    vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    for (auto& frameBuffer : m_frameBuffers) {
        vkDestroyFramebuffer(m_device, frameBuffer, nullptr);
    }

    for (auto& shaderModule : m_shaderModules) {
        vkDestroyShaderModule(m_device, shaderModule, nullptr);
    }
    vkDestroyImageView(m_device, m_depthStencil.view, nullptr);
    vkDestroyImage(m_device, m_depthStencil.image, nullptr);
    vkFreeMemory(m_device, m_depthStencil.mem, nullptr);

    vkDestroyPipelineCache(m_device, m_pipelineCache, nullptr);

    vkDestroyCommandPool(m_device, m_cmdPool, nullptr);

    // Destroy all semaphores (created based on imageCount)
    for (size_t i = 0; i < m_imageAvailableSemaphores.size(); ++i) {
        vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
    }
    for (size_t i = 0; i < m_renderFinishedSemaphores.size(); ++i) {
        vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
    }
    // Destroy all fences (created based on imageCount)
    for (size_t i = 0; i < m_inFlightFences.size(); ++i) {
        vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
    }

    if (m_settings.useCompute) {
        destroyComputeCommandBuffers();
        vkDestroyCommandPool(m_device, m_compute.commandPool, nullptr);
        for (size_t i = 0; i < m_compute.semaphores.size(); ++i) {
            vkDestroySemaphore(m_device, m_compute.semaphores[i], nullptr);
        }
        for (size_t i = 0; i < m_compute.fences.size(); ++i) {
            vkDestroyFence(m_device, m_compute.fences[i], nullptr);
        }
    }

    delete m_vulkanDevice;

    if (m_settings.validation) {
        debug::freeDebugCallback(m_instance);
    }

    vkDestroyInstance(m_instance, nullptr);

    glfwDestroyWindow(m_window);
    glfwTerminate();

    delete m_scene;
}

bool BaseProject::initVulkan()
{
    // Vulkan instance
    VkResult err = createInstance(m_settings.validation);
    if (err) {
        throw std::runtime_error("Could not create Vulkan instance : \n" + tools::errorString(err));
    }

    if (m_settings.validation) {
        // The report flags determine what type of messages for the layers will be
        // displayed For validating (debugging) an application the error and warning
        // bits should suffice
        VkDebugReportFlagsEXT debugReportFlags
            = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
        // Additional flags include performance info, loader and layer debug
        // messages, etc.
        CHECK_RESULT(debug::setupDebugging(m_instance, debugReportFlags, VK_NULL_HANDLE))
    }

    // Physical device
    uint32_t gpuCount = 0;
    // Get number of available physical devices
    CHECK_RESULT(vkEnumeratePhysicalDevices(m_instance, &gpuCount, nullptr))
    assert(gpuCount > 0);
    // Enumerate devices
    std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
    err = vkEnumeratePhysicalDevices(m_instance, &gpuCount, physicalDevices.data());
    if (err) {
        throw std::runtime_error(
            "Could not enumerate physical devices : \n" + tools::errorString(err));
    }

    // GPU selection
    const auto physicalDevice = tools::getBestSuitableDevice(physicalDevices,
        m_enabledDeviceExtensions,
        m_enabledFeatures);

    // Derived classes can override this to set actual features to enable for logical device
    // creation
    getEnabledFeatures();

    // Vulkan device creation
    m_vulkanDevice = new Device(physicalDevice);
    VkResult res = m_vulkanDevice->createLogicalDevice(m_enabledFeatures,
        m_enabledDeviceExtensions,
        m_deviceCreatedNextChain);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("Could not create Vulkan device: \n" + tools::errorString(res));
    }
    m_device = m_vulkanDevice->logicalDevice;

    // Get a graphics queue from the device
    vkGetDeviceQueue(m_device, m_vulkanDevice->queueFamilyIndices.graphics, 0, &m_queue);

    // Find a suitable depth format
    VkBool32 validDepthFormat
        = tools::getSupportedDepthFormat(m_vulkanDevice->physicalDevice, &m_depthFormat);
    assert(validDepthFormat);

    m_swapChain.connect(m_instance, m_vulkanDevice->physicalDevice, m_device);

    return true;
}

void BaseProject::viewChanged() { }

void BaseProject::windowResized() { }

void BaseProject::buildCommandBuffers() { }

void BaseProject::createSynchronizationPrimitives()
{
    // Create synchronization objects
    // Use one semaphore per swapchain image to avoid semaphore reuse issues
    // This ensures each swapchain image has its own semaphore pair
    // Use imageCount for all synchronization primitives to maximize parallelism
    m_imageAvailableSemaphores.resize(m_swapChain.imageCount);
    m_renderFinishedSemaphores.resize(m_swapChain.imageCount);
    m_inFlightFences.resize(m_swapChain.imageCount);
    m_imageToFrameIndex.resize(m_swapChain.imageCount, SIZE_MAX);
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    // Create semaphores for each swapchain image
    for (size_t i = 0; i < m_swapChain.imageCount; ++i) {
        CHECK_RESULT(vkCreateSemaphore(m_device,
            &semaphoreCreateInfo,
            nullptr,
            &m_imageAvailableSemaphores[i]))
        std::string name = "ImageAvailableSemaphore[" + std::to_string(i) + "]";
        debug::setObjectName(m_device,
            (uint64_t)m_imageAvailableSemaphores[i],
            VK_OBJECT_TYPE_SEMAPHORE,
            name.c_str());

        CHECK_RESULT(vkCreateSemaphore(m_device,
            &semaphoreCreateInfo,
            nullptr,
            &m_renderFinishedSemaphores[i]))
        name = "RenderFinishedSemaphore[" + std::to_string(i) + "]";
        debug::setObjectName(m_device,
            (uint64_t)m_renderFinishedSemaphores[i],
            VK_OBJECT_TYPE_SEMAPHORE,
            name.c_str());
    }
    // Create fences for each swapchain image
    for (size_t i = 0; i < m_swapChain.imageCount; ++i) {
        CHECK_RESULT(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_inFlightFences[i]))
        std::string name = "InFlightFence[" + std::to_string(i) + "]";
        debug::setObjectName(m_device,
            (uint64_t)m_inFlightFences[i],
            VK_OBJECT_TYPE_FENCE,
            name.c_str());
    }
}

void BaseProject::destroySynchronizationPrimitives()
{
    // Destroy all semaphores and fences
    for (size_t i = 0; i < m_imageAvailableSemaphores.size(); ++i) {
        vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
    }
    for (size_t i = 0; i < m_renderFinishedSemaphores.size(); ++i) {
        vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
    }
    for (size_t i = 0; i < m_inFlightFences.size(); ++i) {
        vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
    }
    if (m_settings.useCompute) {
        for (size_t i = 0; i < m_compute.semaphores.size(); ++i) {
            vkDestroySemaphore(m_device, m_compute.semaphores[i], nullptr);
        }
        for (size_t i = 0; i < m_compute.fences.size(); ++i) {
            vkDestroyFence(m_device, m_compute.fences[i], nullptr);
        }
    }
}

void BaseProject::recreateComputeSynchronizationPrimitives()
{
    if (!m_settings.useCompute) {
        return;
    }
    VkFenceCreateInfo fenceCreateInfo {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    m_compute.fences.resize(m_swapChain.imageCount);
    m_compute.semaphores.resize(m_swapChain.imageCount);
    for (size_t i = 0; i < m_swapChain.imageCount; ++i) {
        CHECK_RESULT(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_compute.fences[i]))
        std::string name = "ComputeFence[" + std::to_string(i) + "]";
        debug::setObjectName(m_device,
            (uint64_t)m_compute.fences[i],
            VK_OBJECT_TYPE_FENCE,
            name.c_str());

        CHECK_RESULT(
            vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_compute.semaphores[i]))
        name = "ComputeSemaphore[" + std::to_string(i) + "]";
        debug::setObjectName(m_device,
            (uint64_t)m_compute.semaphores[i],
            VK_OBJECT_TYPE_SEMAPHORE,
            name.c_str());
    }
}

void BaseProject::createCommandPool()
{
    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = m_swapChain.queueNodeIndex;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    CHECK_RESULT(vkCreateCommandPool(m_device, &cmdPoolInfo, nullptr, &m_cmdPool))
}

void BaseProject::setupDepthStencil()
{
    VkImageCreateInfo imageCI {};
    imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = m_depthFormat;
    imageCI.extent = { m_width, m_height, 1 };
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    CHECK_RESULT(vkCreateImage(m_device, &imageCI, nullptr, &m_depthStencil.image))
    VkMemoryRequirements memReqs {};
    vkGetImageMemoryRequirements(m_device, m_depthStencil.image, &memReqs);

    VkMemoryAllocateInfo memAlloc {};
    memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = m_vulkanDevice->getMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    CHECK_RESULT(vkAllocateMemory(m_device, &memAlloc, nullptr, &m_depthStencil.mem))
    CHECK_RESULT(vkBindImageMemory(m_device, m_depthStencil.image, m_depthStencil.mem, 0))

    VkImageViewCreateInfo imageViewCI {};
    imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCI.image = m_depthStencil.image;
    imageViewCI.format = m_depthFormat;
    imageViewCI.subresourceRange.baseMipLevel = 0;
    imageViewCI.subresourceRange.levelCount = 1;
    imageViewCI.subresourceRange.baseArrayLayer = 0;
    imageViewCI.subresourceRange.layerCount = 1;
    imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    // Stencil aspect should only be set on depth + stencil formats
    // (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
    if (m_depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
        imageViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    CHECK_RESULT(vkCreateImageView(m_device, &imageViewCI, nullptr, &m_depthStencil.view))
}

void BaseProject::setupFrameBuffer()
{
    VkImageView attachments[2];

    // Depth/Stencil attachment is the same for all frame buffers
    attachments[1] = m_depthStencil.view;

    VkFramebufferCreateInfo frameBufferCreateInfo = {};
    frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frameBufferCreateInfo.pNext = nullptr;
    frameBufferCreateInfo.renderPass = m_renderPass;
    frameBufferCreateInfo.attachmentCount = 2;
    frameBufferCreateInfo.pAttachments = attachments;
    frameBufferCreateInfo.width = m_width;
    frameBufferCreateInfo.height = m_height;
    frameBufferCreateInfo.layers = 1;

    // Create frame buffers for every swap chain image
    m_frameBuffers.resize(m_swapChain.imageCount);
    for (uint32_t i = 0; i < m_frameBuffers.size(); ++i) {
        attachments[0] = m_swapChain.buffers[i].view;
        CHECK_RESULT(
            vkCreateFramebuffer(m_device, &frameBufferCreateInfo, nullptr, &m_frameBuffers[i]))
    }
}

void BaseProject::setupRenderPass()
{
    std::array<VkAttachmentDescription, 2> attachments = {};
    // Color attachment
    attachments[0].format = m_swapChain.colorFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    // Depth attachment
    attachments[1].format = m_depthFormat;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorReference = {};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference = {};
    depthReference.attachment = 1;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subPassDescription = {};
    subPassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subPassDescription.colorAttachmentCount = 1;
    subPassDescription.pColorAttachments = &colorReference;
    subPassDescription.pDepthStencilAttachment = &depthReference;
    subPassDescription.inputAttachmentCount = 0;
    subPassDescription.pInputAttachments = nullptr;
    subPassDescription.preserveAttachmentCount = 0;
    subPassDescription.pPreserveAttachments = nullptr;
    subPassDescription.pResolveAttachments = nullptr;

    // Sub pass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies = {};

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask
        = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask
        = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subPassDescription;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    CHECK_RESULT(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass))
}

void BaseProject::getEnabledFeatures()
{
    if (m_settings.useRayTracing) {
        m_enabledInstanceExtensions.push_back(
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        m_enabledFeatures.fragmentStoresAndAtomics = VK_TRUE;

        m_enabledDeviceExtensions.emplace_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
        m_enabledDeviceExtensions.emplace_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
        m_enabledDeviceExtensions.emplace_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        m_enabledDeviceExtensions.emplace_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        m_enabledDeviceExtensions.emplace_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
        m_enabledDeviceExtensions.emplace_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        m_enabledDeviceExtensions.emplace_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

        // Required for VK_KHR_ray_tracing_pipeline
        m_enabledDeviceExtensions.emplace_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
        // Required by VK_KHR_spirv_1_4
        m_enabledDeviceExtensions.emplace_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);

        m_rayTracingFeatures.bufferDeviceAddressFeatures.sType
            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        m_rayTracingFeatures.bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

        m_rayTracingFeatures.rayTracingPipelineFeatures.sType
            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        m_rayTracingFeatures.rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
        m_rayTracingFeatures.rayTracingPipelineFeatures.pNext
            = &m_rayTracingFeatures.bufferDeviceAddressFeatures;

        m_rayTracingFeatures.accelerationStructureFeatures.sType
            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        m_rayTracingFeatures.accelerationStructureFeatures.accelerationStructure = VK_TRUE;
        m_rayTracingFeatures.accelerationStructureFeatures.pNext
            = &m_rayTracingFeatures.rayTracingPipelineFeatures;

        m_rayTracingFeatures.descriptorIndexingFeatures.sType
            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        m_rayTracingFeatures.descriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
        m_rayTracingFeatures.descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing
            = VK_TRUE;
        m_rayTracingFeatures.descriptorIndexingFeatures.pNext
            = &m_rayTracingFeatures.accelerationStructureFeatures;

        m_deviceCreatedNextChain = &m_rayTracingFeatures.descriptorIndexingFeatures;
    }
}

void BaseProject::handleWindowResize()
{
    if (!m_prepared) {
        return;
    }
    m_prepared = false;

    int currWidth = 0, currHeight = 0;
    glfwGetFramebufferSize(m_window, &currWidth, &currHeight);
    while (currWidth == 0 || currHeight == 0) {
        glfwGetFramebufferSize(m_window, &currWidth, &currHeight);
        glfwWaitEvents();
    }
    // Ensure all operations on the device have been finished before destroying resources
    vkDeviceWaitIdle(m_device);
    m_width = currWidth;
    m_height = currHeight;

    // Save previous image count before recreating swapchain
    size_t previousImageCount = m_swapChain.imageCount;

    // Recreate swap chain (this updates m_swapChain.imageCount)
    setupSwapChain();

    // Only destroy/recreate synchronization primitives if imageCount changed
    if (previousImageCount != m_swapChain.imageCount) {
        destroySynchronizationPrimitives();
        createSynchronizationPrimitives();
        recreateComputeSynchronizationPrimitives();
    }

    // Recreate the frame buffers
    vkDestroyImageView(m_device, m_depthStencil.view, nullptr);
    vkDestroyImage(m_device, m_depthStencil.image, nullptr);
    vkFreeMemory(m_device, m_depthStencil.mem, nullptr);
    setupDepthStencil();
    for (auto& frameBuffer : m_frameBuffers) {
        vkDestroyFramebuffer(m_device, frameBuffer, nullptr);
    }
    setupFrameBuffer();
    // Command buffers need to be recreated as they may store
    // references to the recreated frame buffer
    onSwapChainRecreation();
    destroyCommandBuffers();
    createCommandBuffers();
    if (m_settings.useCompute) {
        destroyComputeCommandBuffers();
        createComputeCommandBuffers();
    }
    buildCommandBuffers();

    vkDeviceWaitIdle(m_device);

    if ((m_width > 0.0f) && (m_height > 0.0f)) {
        m_scene->getCamera()->updateAspectRatio(
            static_cast<float>(m_width) / static_cast<float>(m_height));
    }

    // Notify derived class
    windowResized();
    viewChanged();

    m_prepared = true;
}

void BaseProject::onSwapChainRecreation()
{
    // Can be overridden in derived class
}

void BaseProject::onKeyEvent(int t_key, int t_scancode, int t_action, int t_mods)
{
    // Can be overridden in derived class
}

void BaseProject::initSwapChain() { m_swapChain.initSurface(m_window); }

void BaseProject::setupSwapChain() { m_swapChain.create(&m_width, &m_height, m_settings.vsync); }

void BaseProject::initWindow()
{
#if defined(__linux__)
    // Force X11 platform on Linux to avoid Wayland/libdecor issues
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    if (!glfwVulkanSupported()) {
        throw std::runtime_error("Vulkan is not supported!");
    }
    m_window = glfwCreateWindow(m_width, m_height, m_windowTitle.c_str(), nullptr, nullptr);
}

void BaseProject::setupWindowCallbacks()
{
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
    glfwSetMouseButtonCallback(m_window, mouseCallback);
    glfwSetCursorPosCallback(m_window, mousePositionCallback);
    glfwSetKeyCallback(m_window, keyCallback);
}

void BaseProject::framebufferResizeCallback(GLFWwindow* t_window, int t_width, int t_height)
{
    auto app = reinterpret_cast<BaseProject*>(glfwGetWindowUserPointer(t_window));
    app->m_framebufferResized = true;
}

void BaseProject::mouseCallback(GLFWwindow* t_window, int t_button, int t_action, int t_mods)
{
    auto app = reinterpret_cast<BaseProject*>(glfwGetWindowUserPointer(t_window));
    app->handleMouseClick(t_button, t_action, t_mods);
}

void BaseProject::mousePositionCallback(GLFWwindow* t_window, double t_x, double t_y)
{
    auto app = reinterpret_cast<BaseProject*>(glfwGetWindowUserPointer(t_window));
    double x = 0;
    double y = 0;
    glfwGetCursorPos(t_window, &x, &y);
    app->handleMousePositionChanged(x, y);
}

void BaseProject::keyCallback(GLFWwindow* t_window, int t_key, int t_scancode, int t_action,
    int t_mods)
{
    auto app = reinterpret_cast<BaseProject*>(glfwGetWindowUserPointer(t_window));
    app->handleKeyEvent(t_key, t_scancode, t_action, t_mods);
}

void BaseProject::handleMouseClick(int t_button, int t_action, int t_mods)
{
    if (t_button == GLFW_MOUSE_BUTTON_LEFT) {
        if (GLFW_PRESS == t_action) {
            m_mouseButtons.left = true;
        } else if (GLFW_RELEASE == t_action) {
            m_mouseButtons.left = false;
        }
    }
    if (t_button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (GLFW_PRESS == t_action) {
            m_mouseButtons.right = true;
        } else if (GLFW_RELEASE == t_action) {
            m_mouseButtons.right = false;
        }
    }
    if (t_button == GLFW_MOUSE_BUTTON_MIDDLE) {
        if (GLFW_PRESS == t_action) {
            m_mouseButtons.middle = true;
        } else if (GLFW_RELEASE == t_action) {
            m_mouseButtons.middle = false;
        }
    }
}

void BaseProject::handleMousePositionChanged(int32_t t_x, int32_t t_y)
{
    int32_t dx = static_cast<int32_t>(m_mousePos.x) - t_x;
    int32_t dy = t_y - static_cast<int32_t>(m_mousePos.y); // inverted y coords

    bool handled = false;

    mouseMoved(static_cast<float>(t_x), static_cast<float>(t_y), handled);

    if (handled) {
        m_mousePos = glm::vec2(static_cast<float>(t_x), static_cast<float>(t_y));
        return;
    }

    auto camera = m_scene->getCamera();
    if (m_mouseButtons.left) {
        camera->rotate(dx * camera->rotationSpeed, dy * camera->rotationSpeed);
        m_viewUpdated = true;
    }
    if (m_mouseButtons.right) {
        camera->translate(glm::vec3(0.0f, 0.0f, dy * .5f));
        m_viewUpdated = true;
    }
    if (m_mouseButtons.middle) {
        camera->translate(glm::vec3(dx * 0.01f, dy * 0.01f, 0.0f));
        m_viewUpdated = true;
    }
    m_mousePos = glm::vec2(static_cast<float>(t_x), static_cast<float>(t_y));
}

void BaseProject::handleKeyEvent(int t_key, int t_scancode, int t_action, int t_mods)
{
    auto camera = m_scene->getCamera();
    switch (t_key) {
    case GLFW_KEY_W:
        if (t_action == GLFW_PRESS) {
            camera->keys.up = true;
        } else if (t_action == GLFW_RELEASE) {
            camera->keys.up = false;
        }
        break;
    case GLFW_KEY_A:
        if (t_action == GLFW_PRESS) {
            camera->keys.left = true;
        } else if (t_action == GLFW_RELEASE) {
            camera->keys.left = false;
        }
        break;
    case GLFW_KEY_S:
        if (t_action == GLFW_PRESS) {
            camera->keys.down = true;
        } else if (t_action == GLFW_RELEASE) {
            camera->keys.down = false;
        }
        break;
    case GLFW_KEY_D:
        if (t_action == GLFW_PRESS) {
            camera->keys.right = true;
        } else if (t_action == GLFW_RELEASE) {
            camera->keys.right = false;
        }
        break;
    default:
        break;
    }
    onKeyEvent(t_key, t_scancode, t_action, t_mods);
}

void BaseProject::run()
{
    initWindow();
    initVulkan();
    prepare();
    setupWindowCallbacks();
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        if (m_prepared) {
            nextFrame();
        }
    }
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
    }
}

void BaseProject::mouseMoved(double t_x, double t_y, bool& t_handled) { }
