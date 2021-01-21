/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_TEXTURE_H
#define MANUEME_TEXTURE_H

#include <FreeImage.h>
#include <assimp/texture.h>

#include "../base_project.h"
#include "device.h"
#include "vulkan/vulkan_core.h"

class Texture {
public:
    Texture();
    virtual ~Texture();

    /** @brief Release all Vulkan resources of this texture,
     * this function is NOT called by the destructor of the class */
    void destroy();

    void updateDescriptor();

    static FIBITMAP* loadBitmap(const std::string& t_path);

    VkDescriptorImageInfo descriptor;

protected:
    Device* m_device;
    VkImage m_image;
    VkImageLayout m_imageLayout;
    VkDeviceMemory m_deviceMemory;
    VkImageView m_view;
    uint32_t m_width, m_height;
    VkSampler m_sampler;
};

class VulkanTexture2D : public Texture {
public:
    VulkanTexture2D();

    ~VulkanTexture2D() override;

    void loadFromFile(const std::string& t_filename, VkFormat t_format, Device* t_device,
        VkQueue t_copyQueue, VkImageUsageFlags t_imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageLayout t_imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        bool t_forceLinear = false, VkImageTiling t_tiling = VK_IMAGE_TILING_OPTIMAL);

    void loadFromAssimp(const aiTexture* t_texture, VkFormat t_format, Device* t_device,
        VkQueue t_copyQueue, VkImageUsageFlags t_imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageLayout t_imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        bool t_forceLinear = false, VkImageTiling t_tiling = VK_IMAGE_TILING_OPTIMAL);

    void loadFromFibitmap(FIBITMAP* t_fibitmap, VkFormat t_format, Device* t_device,
        VkQueue t_copyQueue, VkImageUsageFlags t_imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageLayout t_imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        bool t_forceLinear = false, VkImageTiling t_tiling = VK_IMAGE_TILING_OPTIMAL);

    void fromBuffer(void* t_buffer, VkDeviceSize t_bufferSize, VkFormat t_format,
        uint32_t t_texWidth, uint32_t t_texHeight, Device* t_device, VkQueue t_copyQueue,
        VkFilter t_filter = VK_FILTER_LINEAR,
        VkImageUsageFlags t_imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageLayout t_imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    void fromNothing(VkFormat t_format, uint32_t t_texWidth, uint32_t t_texHeight, Device* t_device,
        VkQueue t_copyQueue, VkFilter t_filter = VK_FILTER_LINEAR,
        VkImageUsageFlags t_imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageLayout t_imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        VkImageAspectFlags t_aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);

    void depthAttachment(VkFormat t_format, uint32_t t_texWidth, uint32_t t_texHeight,
        Device* t_device, VkQueue t_copyQueue, VkImageUsageFlags t_imageUsageFlags = 0);

    void colorAttachment(VkFormat t_format, uint32_t t_texWidth, uint32_t t_texHeight,
        Device* t_device, VkQueue t_copyQueue, VkImageUsageFlags t_imageUsageFlags = 0);
};

#endif // MANUEME_TEXTURE_H
