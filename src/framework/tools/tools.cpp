/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "tools.h"

namespace tools {
std::string errorString(VkResult t_errorCode)
{
    switch (t_errorCode) {
#define STR(r)                                                                                     \
    case VK_##r:                                                                                   \
        return #r
        STR(NOT_READY);
        STR(TIMEOUT);
        STR(EVENT_SET);
        STR(EVENT_RESET);
        STR(INCOMPLETE);
        STR(ERROR_OUT_OF_HOST_MEMORY);
        STR(ERROR_OUT_OF_DEVICE_MEMORY);
        STR(ERROR_INITIALIZATION_FAILED);
        STR(ERROR_DEVICE_LOST);
        STR(ERROR_MEMORY_MAP_FAILED);
        STR(ERROR_LAYER_NOT_PRESENT);
        STR(ERROR_EXTENSION_NOT_PRESENT);
        STR(ERROR_FEATURE_NOT_PRESENT);
        STR(ERROR_INCOMPATIBLE_DRIVER);
        STR(ERROR_TOO_MANY_OBJECTS);
        STR(ERROR_FORMAT_NOT_SUPPORTED);
        STR(ERROR_SURFACE_LOST_KHR);
        STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
        STR(SUBOPTIMAL_KHR);
        STR(ERROR_OUT_OF_DATE_KHR);
        STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
        STR(ERROR_VALIDATION_FAILED_EXT);
        STR(ERROR_INVALID_SHADER_NV);
        STR(ERROR_OUT_OF_POOL_MEMORY);
#undef STR
    default:
        return "UNKNOWN_ERROR";
    }
}

VkBool32 getSupportedDepthFormat(VkPhysicalDevice t_physicalDevice, VkFormat* t_depthFormat)
{
    // Since all depth formats may be optional, we need to find a suitable depth
    // format to use Start with the highest precision packed format
    std::vector<VkFormat> depthFormats = { VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM };

    for (auto& format : depthFormats) {
        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(t_physicalDevice, format, &formatProps);
        // Format must support depth stencil attachment for optimal tiling
        if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            *t_depthFormat = format;
            return true;
        }
    }

    return false;
}

// Create an image memory barrier for changing the layout of
// an image and put it into an active command buffer
void setImageLayout(VkCommandBuffer t_cmdbuffer, VkImage t_image, VkImageLayout t_oldImageLayout,
    VkImageLayout t_newImageLayout, VkImageSubresourceRange t_subresourceRange,
    VkPipelineStageFlags t_srcStageMask, VkPipelineStageFlags t_dstStageMask)
{
    // Create an image barrier object
    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.oldLayout = t_oldImageLayout;
    imageMemoryBarrier.newLayout = t_newImageLayout;
    imageMemoryBarrier.image = t_image;
    imageMemoryBarrier.subresourceRange = t_subresourceRange;

    // Source layouts (old)
    // Source access mask controls actions that have to be finished on the old
    // layout before it will be transitioned to the new layout
    switch (t_oldImageLayout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        // Image layout is undefined (or does not matter)
        // Only valid as initial layout
        // No flags required, listed only for completeness
        imageMemoryBarrier.srcAccessMask = 0;
        break;

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        // Image is preinitialized
        // Only valid as initial layout for linear images, preserves memory
        // contents Make sure host writes have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Image is a color attachment
        // Make sure any writes to the color buffer have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // Image is a depth/stencil attachment
        // Make sure any writes to the depth/stencil buffer have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // Image is a transfer source
        // Make sure any reads from the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        // Image is a transfer destination
        // Make sure any writes to the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // Image is read by a shader
        // Make sure any shader reads from the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        // Other source layouts aren't handled (yet)
        break;
    }

    // Target layouts (new)
    // Destination access mask controls the dependency for the new image layout
    switch (t_newImageLayout) {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        // Image will be used as a transfer destination
        // Make sure any writes to the image have been finished
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // Image will be used as a transfer source
        // Make sure any reads from the image have been finished
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Image will be used as a color attachment
        // Make sure any writes to the color buffer have been finished
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // Image layout will be used as a depth/stencil attachment
        // Make sure any writes to depth/stencil buffer have been finished
        imageMemoryBarrier.dstAccessMask
            = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // Image will be read in a shader (sampler, input attachment)
        // Make sure any writes to the image have been finished
        if (imageMemoryBarrier.srcAccessMask == 0) {
            imageMemoryBarrier.srcAccessMask
                = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        // Other source layouts aren't handled (yet)
        break;
    }

    // Put barrier inside setup command buffer
    vkCmdPipelineBarrier(t_cmdbuffer,
        t_srcStageMask,
        t_dstStageMask,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &imageMemoryBarrier);
}

// Fixed sub resource on first mip level and layer
void setImageLayout(VkCommandBuffer t_cmdbuffer, VkImage t_image, VkImageAspectFlags t_aspectMask,
    VkImageLayout t_oldImageLayout, VkImageLayout t_newImageLayout,
    VkPipelineStageFlags t_srcStageMask, VkPipelineStageFlags t_dstStageMask)
{
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = t_aspectMask;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 1;
    setImageLayout(t_cmdbuffer,
        t_image,
        t_oldImageLayout,
        t_newImageLayout,
        subresourceRange,
        t_srcStageMask,
        t_dstStageMask);
}

void insertImageMemoryBarrier(
    VkCommandBuffer cmdbuffer,
    VkImage image,
    VkAccessFlags srcAccessMask,
    VkAccessFlags dstAccessMask,
    VkImageLayout oldImageLayout,
    VkImageLayout newImageLayout,
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    VkImageSubresourceRange subresourceRange)
{
    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.srcAccessMask = srcAccessMask;
    imageMemoryBarrier.dstAccessMask = dstAccessMask;
    imageMemoryBarrier.oldLayout = oldImageLayout;
    imageMemoryBarrier.newLayout = newImageLayout;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange = subresourceRange;

    vkCmdPipelineBarrier(
        cmdbuffer,
        srcStageMask,
        dstStageMask,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);
}

VkShaderModule loadShader(const char* t_fileName, VkDevice t_device)
{
    std::ifstream is(t_fileName, std::ios::binary | std::ios::in | std::ios::ate);

    if (is.is_open()) {
        size_t size = is.tellg();
        is.seekg(0, std::ios::beg);
        char* shaderCode = new char[size];
        is.read(shaderCode, size);
        is.close();

        assert(size > 0);

        VkShaderModule shaderModule = nullptr;
        VkShaderModuleCreateInfo moduleCreateInfo {};
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.codeSize = size;
        moduleCreateInfo.pCode = reinterpret_cast<uint32_t*>(shaderCode);

        CHECK_RESULT(vkCreateShaderModule(t_device, &moduleCreateInfo, nullptr, &shaderModule));

        delete[] shaderCode;

        return shaderModule;
    } else {
        throw std::runtime_error(
            "Error: Could not open shader file \"" + std::string(t_fileName) + "\"");
    }
}

void checkDeviceExtensionSupport(
    VkPhysicalDevice t_device, const std::vector<const char*>& t_extensionList)
{
    uint32_t supportedExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(t_device, nullptr, &supportedExtensionCount, nullptr);
    std::vector<VkExtensionProperties> supportedExtensions(supportedExtensionCount);
    //	Each VkExtensionProperties struct contains the name and version of an
    // ex-tension.
    vkEnumerateDeviceExtensionProperties(t_device,
        nullptr,
        &supportedExtensionCount,
        supportedExtensions.data());
    for (auto requiredExtensionName : t_extensionList) {
        auto iterator = std::find_if(supportedExtensions.begin(),
            supportedExtensions.end(),
            [requiredExtensionName](const VkExtensionProperties& t_extension) {
                return strcmp(t_extension.extensionName, requiredExtensionName) == 0;
            });
        if (iterator == supportedExtensions.end()) {
            throw std::runtime_error(
                (std::string) "unsupported extension " + requiredExtensionName);
        }
    }
}

bool validateDeviceExtensionSupport(
    VkPhysicalDevice t_device, const std::vector<const char*>& t_extensionList)
{
    try {
        checkDeviceExtensionSupport(t_device, t_extensionList);
        return true;
    } catch (std::exception) {
        return false;
    }
}

bool hasRequiredFeatures(
    VkPhysicalDevice t_physicalDevice, VkPhysicalDeviceFeatures t_requiredFeatures)
{
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(t_physicalDevice, &deviceFeatures);
    return // TODO: Add here the list of features to test, it doesn't make sense
           // to validate all for now
        (!t_requiredFeatures.geometryShader || deviceFeatures.geometryShader)
        && (!t_requiredFeatures.samplerAnisotropy || deviceFeatures.samplerAnisotropy)
        && (!t_requiredFeatures.tessellationShader || deviceFeatures.tessellationShader);
}

bool isDeviceSuitable(VkPhysicalDevice t_physicalDevice,
    const std::vector<const char*>& t_requiredExtensions,
    const VkPhysicalDeviceFeatures& t_requiredFeatures)
{
    if (!validateDeviceExtensionSupport(t_physicalDevice, t_requiredExtensions)) {
        return false;
    }
    return hasRequiredFeatures(t_physicalDevice, t_requiredFeatures);
}

int rateDeviceSuitability(VkPhysicalDevice t_physicalDevice,
    const std::vector<const char*>& t_requiredExtensions,
    const VkPhysicalDeviceFeatures& t_requiredFeatures)
{
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(t_physicalDevice, &deviceProperties);
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(t_physicalDevice, &deviceFeatures);

    // Application can't function without this:
    if (!isDeviceSuitable(t_physicalDevice, t_requiredExtensions, t_requiredFeatures)) {
        return 0;
    }

    int score = 0;
    // Discrete GPUs have a significant performance advantage
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    } else if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        score += 500;
    }
    // Maximum possible size of textures affects graphics quality
    score += deviceProperties.limits.maxImageDimension2D;

    return score;
}

VkPhysicalDevice getBestSuitableDevice(const std::vector<VkPhysicalDevice>& t_devices,
    const std::vector<const char*>& t_requiredExtensions,
    const VkPhysicalDeviceFeatures& t_requiredFeatures)
{
    std::multimap<int, VkPhysicalDevice> candidates;
    for (const auto& device : t_devices) {
        int score = rateDeviceSuitability(device, t_requiredExtensions, t_requiredFeatures);
        candidates.insert(std::make_pair(score, device));
    }
    if (candidates.rbegin()->first > 0) {
        return candidates.rbegin()->second;
    } else {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

uint32_t alignedSize(uint32_t t_value, uint32_t t_alignment)
{
    return (t_value + t_alignment - 1) & ~(t_alignment - 1);
}

} // namespace tools