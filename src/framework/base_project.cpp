/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "base_project.h"

#include "scene/scene.h"
#include <cmath>
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

VkResult BaseProject::renderFrame()
{
    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
    // Acquire the next image from the swap chain
    uint32_t imageIndex = 0;
    VkResult result
        = m_swapChain.acquireNextImage(m_imageAvailableSemaphores[m_currentFrame], &imageIndex);
    // Recreate the SwapChain if it's no longer compatible with the surface
    // (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
    if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR)) {
        handleWindowResize();
    } else {
        CHECK_RESULT(result)
    }

    // Check if a previous frame is using this image (i.e. there is its fence to
    // wait on)
    if (m_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(m_device, 1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    // Mark the image as now being in use by this frame
    m_imagesInFlight[imageIndex] = m_inFlightFences[m_currentFrame];

    updateUniformBuffers(imageIndex);

    // Submitting the command buffer
    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    std::array<VkSemaphore, 1> waitSemaphores = { m_imageAvailableSemaphores[m_currentFrame] };
    std::array<VkSemaphore, 1> signalSemaphores = { m_renderFinishedSemaphores[m_currentFrame] };
    std::array<VkPipelineStageFlags, 1> waitStages
        = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_drawCmdBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores.data();

    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);

    CHECK_RESULT(vkQueueSubmit(m_queue, 1, &submitInfo, m_inFlightFences[m_currentFrame]))
    result = m_swapChain.queuePresent(m_queue, imageIndex, signalSemaphores.data());
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
        m_framebufferResized = false;
        handleWindowResize();
    } else if (result != VK_SUCCESS) {
        CHECK_RESULT(result)
    }
    m_currentFrame = (m_currentFrame + 1) % m_maxFramesInFlight;
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

    CHECK_RESULT(
        vkAllocateCommandBuffers(m_device, &cmdBufAllocateInfo, m_drawCmdBuffers.data()))
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
}

VkPipelineShaderStageCreateInfo BaseProject::loadShader(
    const std::string& t_fileName, VkShaderStageFlagBits t_stage)
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
    m_frameCounter++;
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

    for (size_t i = 0; i < m_maxFramesInFlight; i++) {
        vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
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
    m_physicalDevice = tools::getBestSuitableDevice(physicalDevices,
        m_enabledDeviceExtensions,
        m_enabledFeatures);
    // Store properties (including limits), features and memory properties of the
    // physical device
    vkGetPhysicalDeviceProperties(m_physicalDevice, &m_deviceProperties);
    vkGetPhysicalDeviceFeatures(m_physicalDevice, &m_deviceFeatures);
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_deviceMemoryProperties);

    // Derived classes can override this to set actual features to enable for logical device
    // creation
    getEnabledFeatures();

    // Vulkan device creation
    m_vulkanDevice = new Device(m_physicalDevice);
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
    VkBool32 validDepthFormat = tools::getSupportedDepthFormat(m_physicalDevice, &m_depthFormat);
    assert(validDepthFormat);

    m_swapChain.connect(m_instance, m_physicalDevice, m_device);

    return true;
}

void BaseProject::viewChanged() { }

void BaseProject::windowResized() { }

void BaseProject::buildCommandBuffers() { }

void BaseProject::createSynchronizationPrimitives()
{
    // Create synchronization objects
    m_imageAvailableSemaphores.resize(m_maxFramesInFlight);
    m_renderFinishedSemaphores.resize(m_maxFramesInFlight);
    m_inFlightFences.resize(m_maxFramesInFlight);
    m_imagesInFlight.resize(m_swapChain.imageCount, VK_NULL_HANDLE);
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (size_t i = 0; i < m_maxFramesInFlight; i++) {
        CHECK_RESULT(vkCreateSemaphore(m_device,
            &semaphoreCreateInfo,
            nullptr,
            &m_imageAvailableSemaphores[i]))
        CHECK_RESULT(vkCreateSemaphore(m_device,
            &semaphoreCreateInfo,
            nullptr,
            &m_renderFinishedSemaphores[i]))
        CHECK_RESULT(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_inFlightFences[i]))
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
    for (uint32_t i = 0; i < m_frameBuffers.size(); i++) {
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
    // Can be overridden in derived class
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
    // Ensure all operations on the device have been finished before destroying
    // resources
    vkDeviceWaitIdle(m_device);
    m_width = currWidth;
    m_height = currHeight;

    // Recreate swap chain
    setupSwapChain();

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

void BaseProject::keyCallback(
    GLFWwindow* t_window, int t_key, int t_scancode, int t_action, int t_mods)
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

void BaseProject::updateUniformBuffers(uint32_t t_currentImage) { }
