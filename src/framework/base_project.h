/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_BASE_PROJECT_H
#define MANUEME_BASE_PROJECT_H

#include <sys/stat.h>

#include <chrono>
#include <iostream>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <array>
#include <glm/glm.hpp>
#include <numeric>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "core/device.h"
#include "core/swapchain.h"
#include "scene/camera.h"
#include "tools/debug.h"
#include "tools/initializers.hpp"
#include "tools/tools.h"

class Scene;

class BaseProject {
public:
    explicit BaseProject(std::string t_appName, std::string t_windowTitle,
        bool t_enableValidation = false);
    virtual ~BaseProject();
    void run();

private:
    // Device extra features
    struct {
        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures {};
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures {};
        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures {};
        VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexingFeatures {};
    } m_rayTracingFeatures;

    GLFWwindow* m_window;
    bool m_viewUpdated = true;
    bool m_framebufferResized = false;

    glm::vec2 m_mousePos;
    struct {
        bool left = false;
        bool right = false;
        bool middle = false;
    } m_mouseButtons;

    void initWindow();
    void nextFrame();

    // Callbacks and Event handling
    void setupWindowCallbacks();
    static void framebufferResizeCallback(GLFWwindow* t_window, int t_width, int t_height);
    static void mouseCallback(GLFWwindow* t_window, int t_button, int t_action, int t_mods);
    static void mousePositionCallback(GLFWwindow* t_window, double t_x, double t_y);
    static void keyCallback(GLFWwindow* t_window, int t_key, int t_scancode, int t_action,
        int t_mods);
    void handleMouseClick(int t_button, int t_action, int t_mods);
    void handleMousePositionChanged(int32_t t_x, int32_t t_y);
    void handleKeyEvent(int t_key, int t_scancode, int t_action, int t_mods);
    void handleWindowResize();

protected:
    // constants
    const VkClearColorValue m_default_clear_color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    const uint32_t api_version = VK_API_VERSION_1_2;
    // --

    std::string m_windowTitle;
    std::string m_appName;

    bool m_prepared = false;
    uint32_t m_width = 1280;
    uint32_t m_height = 720;

    struct {
        VkImage image;
        VkDeviceMemory mem;
        VkImageView view;
    } m_depthStencil;

    // Scene with models, vertex, index, materials, textures, instances, etc.
    Scene* m_scene;

    uint32_t m_frameCounter = 0;
    uint32_t m_lastFps = 0;
    float m_frameTimer = 1.0f;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_lastTimestamp;

    // Vulkan instance, stores all per-application states
    VkInstance m_instance;

    // Encapsulated physical and logical vulkan device
    Device* m_vulkanDevice;

    // Logical device, application's view of the physical device (GPU)
    VkDevice m_device;

    // Set of physical device features to be enabled (must be set in the derived constructor)
    VkPhysicalDeviceFeatures m_enabledFeatures {};

    // Optional pNext structure for passing extension structures to device creation
    void* m_deviceCreatedNextChain = nullptr;

    // Set of device extensions to be enabled (must be set in the derived constructor)
    std::vector<const char*> m_enabledDeviceExtensions;
    std::vector<const char*> m_enabledInstanceExtensions;

    // Handle to the device graphics queue that command buffers are submitted to
    VkQueue m_queue;

    // Depth buffer format (selected during Vulkan initialization)
    VkFormat m_depthFormat;

    // Command buffer pool
    VkCommandPool m_cmdPool;

    // Command buffers used for rendering
    std::vector<VkCommandBuffer> m_drawCmdBuffers;

    // Global render pass for frame buffer writes
    VkRenderPass m_renderPass;

    // List of available frame buffers (same as number of swap chain images)
    std::vector<VkFramebuffer> m_frameBuffers;

    // Descriptor set pool
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;

    // List of shader modules created (stored for cleanup)
    std::vector<VkShaderModule> m_shaderModules;

    // Pipeline cache object
    VkPipelineCache m_pipelineCache;

    // Wraps the swap chain to present images (framebuffers) to the windowing system
    SwapChain m_swapChain;

    // Synchronization
    uint32_t m_maxFramesInFlight = 3;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    std::vector<VkFence> m_imagesInFlight;
    size_t m_currentFrame = 0;

    // Separated compute queue, commandPool, fence and buffer
    struct {
        VkQueue queue;
        VkCommandPool commandPool;
        std::vector<VkCommandBuffer> commandBuffers;
        std::vector<VkFence> fences;
        std::vector<VkSemaphore> semaphores;
    } m_compute;

    // Settings
    struct Settings {
        bool validation = false;
        bool vsync = false;
        bool useCompute = false;
        bool useRayTracing = false;
    } m_settings;

    /** @brief Setup the vulkan instance, enable required extensions and connect
     * to the physical device (GPU) */
    bool initVulkan();

    /** @brief Creates the application wide Vulkan instance */
    virtual VkResult createInstance(bool t_enableValidation);

    /** @brief Render function to be implemented by the application */
    virtual void render() = 0;

    /** @brief Called when the camera view has changed */
    virtual void viewChanged();

    /** @brief Called when the window has been resized, can be used by
     * the application to recreate resources (GPU not idle) */
    virtual void windowResized();

    /** @brief Called while the swap chain is recreated and the GPU is
     * idle, can be used by the application to recreate resources */
    virtual void onSwapChainRecreation();

    /** @brief Called when resources have been recreated that require a
     * rebuild of the command buffers (e.g. frame buffer), to be implemente by the
     * application */
    virtual void buildCommandBuffers();

    /** @brief Setup default depth and stencil views */
    virtual void setupDepthStencil();

    /** @brief Setup default framebuffers for all requested swapchain
     * images */
    virtual void setupFrameBuffer();

    /** @brief Setup a default renderpass */
    virtual void setupRenderPass();

    /** @brief Called after the physical device features have been read,
     * can be used to set features to enable on the device */
    virtual void getEnabledFeatures();

    /** @brief Prepares all Vulkan resources and functions required */
    virtual void prepare();
    void createCommandPool();
    void createPipelineCache();
    void createSynchronizationPrimitives();
    void initSwapChain();
    void setupSwapChain();
    void createCommandBuffers();
    void destroyCommandBuffers();

    /** @brief Prepares all Vulkan resources and functions required for the compute pipeline */
    void prepareCompute();
    void createComputeCommandBuffers();
    void destroyComputeCommandBuffers();

    /** @brief Loads a SPIR-V shader file for the given shader stage */
    VkPipelineShaderStageCreateInfo loadShader(const std::string& t_fileName,
        VkShaderStageFlagBits t_stage);

    /** @brief Acquires the next swap chain image to render to. T o submit your command buffer, use
     * m_imageAvailableSemaphores as a wait semaphore after calling this function.
     *  @returns The image index in the swapChain, you will need this index to run
     * queuePresentSwapChain */
    uint32_t acquireNextImage();

    /** @brief Presents the acquired swap chain image waiting for m_renderFinishedSemaphores, your
     * last command submitted must have m_renderFinishedSemaphores[t_imageIndex] as a signal
     * semaphore.
     *  @returns The result of the present operation */
    VkResult queuePresentSwapChain(uint32_t t_imageIndex);

    /** @brief Called when the mouse is moved */
    virtual void mouseMoved(double t_x, double t_y, bool& t_handled);

    /** @brief Called when a key is pressed */
    virtual void onKeyEvent(int t_key, int t_scancode, int t_action, int t_mods);

    void saveScreenshot(const char* filename);
};

#endif // MANUEME_BASE_PROJECT_H
