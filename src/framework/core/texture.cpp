/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "texture.h"

#include <array>

Texture::Texture() { }

Texture::~Texture() = default;

void Texture::updateDescriptor()
{
    descriptor.sampler = m_sampler;
    descriptor.imageView = m_view;
    descriptor.imageLayout = m_imageLayout;
}

FIBITMAP* Texture::loadBitmap(const std::string& t_path)
{
    const auto format = FreeImage_GetFileType(t_path.c_str(), 0);
    if (format == FIF_UNKNOWN) {
        throw std::runtime_error("failed to load texture!");
    }
    const auto bitmap = FreeImage_Load(format, t_path.c_str());
    if (!bitmap) {
        throw std::runtime_error("failed to load texture!");
    }
    const auto bitmap32 = FreeImage_ConvertTo32Bits(bitmap);
    if (!bitmap32) {
        throw std::runtime_error("failed to load texture!");
    }
    FreeImage_Unload(bitmap);
    return bitmap32;
}

void Texture::destroy()
{
    vkDestroyImageView(m_device->logicalDevice, m_view, nullptr);
    vkDestroyImage(m_device->logicalDevice, m_image, nullptr);
    if (m_sampler) {
        vkDestroySampler(m_device->logicalDevice, m_sampler, nullptr);
    }
    vkFreeMemory(m_device->logicalDevice, m_deviceMemory, nullptr);
}

VkImageView Texture::getImageView() { return m_view; }

VkImage Texture::getImage() { return m_image; }

void VulkanTexture2D::loadFromFile(const std::string& t_filename, VkFormat t_format,
    Device* t_device, VkQueue t_copyQueue, VkImageUsageFlags t_imageUsageFlags,
    VkImageLayout t_imageLayout, bool t_forceLinear, VkImageTiling t_tiling)
{
    auto fibitmap = loadBitmap(t_filename);
    loadFromFibitmap(fibitmap,
        t_format,
        t_device,
        t_copyQueue,
        t_imageUsageFlags,
        t_imageLayout,
        t_forceLinear,
        t_tiling);
    FreeImage_Unload(fibitmap);
}

void VulkanTexture2D::fromBuffer(void* t_buffer, VkDeviceSize t_bufferSize, VkFormat t_format,
    uint32_t t_texWidth, uint32_t t_texHeight, Device* t_device, VkQueue t_copyQueue,
    VkFilter t_filter, VkImageUsageFlags t_imageUsageFlags, VkImageLayout t_imageLayout)
{
    assert(t_buffer);

    this->m_device = t_device;
    m_width = t_texWidth;
    m_height = t_texHeight;

    VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VkMemoryRequirements memReqs;

    // Use a separate command buffer for texture loading
    VkCommandBuffer copyCmd = t_device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // Create a host-visible staging buffer that contains the raw image data
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = t_bufferSize;
    // This buffer is used as a transfer source for the buffer copy
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VKM_CHECK_RESULT(
        vkCreateBuffer(t_device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

    // Get memory requirements for the staging buffer (alignment, memory type
    // bits)
    vkGetBufferMemoryRequirements(t_device->logicalDevice, stagingBuffer, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;
    // Get memory type index for a host visible buffer
    memAllocInfo.memoryTypeIndex = t_device->getMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VKM_CHECK_RESULT(
        vkAllocateMemory(t_device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
    VKM_CHECK_RESULT(vkBindBufferMemory(t_device->logicalDevice, stagingBuffer, stagingMemory, 0));

    // Copy texture data into staging buffer
    uint8_t* data;
    VKM_CHECK_RESULT(
        vkMapMemory(t_device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
    memcpy(data, t_buffer, t_bufferSize);
    vkUnmapMemory(t_device->logicalDevice, stagingMemory);

    VkBufferImageCopy bufferCopyRegion = {};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = m_width;
    bufferCopyRegion.imageExtent.height = m_height;
    bufferCopyRegion.imageExtent.depth = 1;
    bufferCopyRegion.bufferOffset = 0;

    // Create optimal tiled target image
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = t_format;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent = { m_width, m_height, 1 };
    imageCreateInfo.usage = t_imageUsageFlags;
    // Ensure that the TRANSFER_DST bit is set for staging
    if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
        imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    VKM_CHECK_RESULT(vkCreateImage(t_device->logicalDevice, &imageCreateInfo, nullptr, &m_image));

    vkGetImageMemoryRequirements(t_device->logicalDevice, m_image, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;

    memAllocInfo.memoryTypeIndex
        = t_device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VKM_CHECK_RESULT(
        vkAllocateMemory(t_device->logicalDevice, &memAllocInfo, nullptr, &m_deviceMemory));
    VKM_CHECK_RESULT(vkBindImageMemory(t_device->logicalDevice, m_image, m_deviceMemory, 0));

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 1;

    // Image barrier for optimal image (target)
    // Optimal image will be used as destination for the copy
    tools::setImageLayout(copyCmd,
        m_image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        subresourceRange);

    // Copy mip levels from staging buffer
    vkCmdCopyBufferToImage(copyCmd,
        stagingBuffer,
        m_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &bufferCopyRegion);

    // Change texture image layout to shader read after all mip levels have been
    // copied
    this->m_imageLayout = t_imageLayout;
    tools::setImageLayout(copyCmd,
        m_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        t_imageLayout,
        subresourceRange);

    t_device->flushCommandBuffer(copyCmd, t_copyQueue);

    // Clean up staging resources
    vkFreeMemory(t_device->logicalDevice, stagingMemory, nullptr);
    vkDestroyBuffer(t_device->logicalDevice, stagingBuffer, nullptr);

    // Create sampler
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = t_filter;
    samplerCreateInfo.minFilter = t_filter;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 0.0f;
    samplerCreateInfo.maxAnisotropy = 1.0f;
    VKM_CHECK_RESULT(
        vkCreateSampler(t_device->logicalDevice, &samplerCreateInfo, nullptr, &m_sampler));

    // Create image view
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.pNext = nullptr;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = t_format;
    viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.image = m_image;
    VKM_CHECK_RESULT(vkCreateImageView(t_device->logicalDevice, &viewCreateInfo, nullptr, &m_view));

    // Update descriptor image info member that can be used for setting up
    // descriptor sets
    updateDescriptor();
}

void VulkanTexture2D::loadFromAssimp(const aiTexture* t_texture, VkFormat t_format,
    Device* t_device, VkQueue t_copyQueue, VkImageUsageFlags t_imageUsageFlags,
    VkImageLayout t_imageLayout, bool t_forceLinear, VkImageTiling t_tiling)
{
    auto fiMemory
        = FreeImage_OpenMemory(reinterpret_cast<BYTE*>(t_texture->pcData), t_texture->mWidth);
    auto format = FIF_UNKNOWN;
    if (t_texture->CheckFormat("jpg")) {
        format = FIF_JPEG;
    }
    if (t_texture->CheckFormat("png")) {
        format = FIF_PNG;
    }
    if (t_texture->CheckFormat("tga")) {
        format = FIF_TARGA;
    }
    auto fibitmap = FreeImage_LoadFromMemory(format, fiMemory);
    const auto bitmap32 = FreeImage_ConvertTo32Bits(fibitmap);
    loadFromFibitmap(bitmap32,
        t_format,
        t_device,
        t_copyQueue,
        t_imageUsageFlags,
        t_imageLayout,
        t_forceLinear,
        t_tiling);
    FreeImage_Unload(fibitmap);
    FreeImage_Unload(bitmap32);
    FreeImage_CloseMemory(fiMemory);
}

void VulkanTexture2D::loadFromFibitmap(FIBITMAP* t_fibitmap, VkFormat t_format, Device* t_device,
    VkQueue t_copyQueue, VkImageUsageFlags t_imageUsageFlags, VkImageLayout t_imageLayout,
    bool t_forceLinear, VkImageTiling t_tiling)
{
    this->m_device = t_device;
    m_width = FreeImage_GetWidth(t_fibitmap);
    m_height = FreeImage_GetHeight(t_fibitmap);
    const auto texChannels = FreeImage_GetBPP(t_fibitmap) / 8;
    VkDeviceSize imageSize = m_width * m_height * texChannels;

    // Get device properites for the requested texture format
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(t_device->physicalDevice, t_format, &formatProperties);

    // Only use linear tiling if requested (and supported by the device)
    // Support for linear tiling is mostly limited, so prefer to use
    // optimal tiling instead
    // On most implementations linear tiling will only support a very
    // limited amount of formats and features (mip maps, cubemaps, arrays, etc.)
    VkBool32 useStaging = !t_forceLinear;

    VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VkMemoryRequirements memReqs;

    // Use a separate command buffer for texture loading
    VkCommandBuffer copyCmd = t_device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    if (useStaging) {
        // Create a host-visible staging buffer that contains the raw image data
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = imageSize;
        // This buffer is used as a transfer source for the buffer copy
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VKM_CHECK_RESULT(
            vkCreateBuffer(t_device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

        // Get memory requirements for the staging buffer (alignment, memory type
        // bits)
        vkGetBufferMemoryRequirements(t_device->logicalDevice, stagingBuffer, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;
        // Get memory type index for a host visible buffer
        memAllocInfo.memoryTypeIndex = t_device->getMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VKM_CHECK_RESULT(
            vkAllocateMemory(t_device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
        VKM_CHECK_RESULT(
            vkBindBufferMemory(t_device->logicalDevice, stagingBuffer, stagingMemory, 0));

        // Copy texture data into staging buffer
        uint8_t* data;
        VKM_CHECK_RESULT(
            vkMapMemory(t_device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
        memcpy(data, FreeImage_GetBits(t_fibitmap), imageSize);
        vkUnmapMemory(t_device->logicalDevice, stagingMemory);

        std::vector<VkBufferImageCopy> bufferCopyRegions;
        // TODO support mipmaps or generate them automatically from original image
        // with vkCmdBlitImage Setup buffer copy regions for each mip level
        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { m_width, m_height, 1 };
        bufferCopyRegions.push_back(region);

        // Create optimal tiled target image
        VkImageCreateInfo imageCreateInfo = {};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = t_format;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = t_tiling;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent = { m_width, m_height, 1 };
        imageCreateInfo.usage = t_imageUsageFlags;
        // Ensure that the TRANSFER_DST bit is set for staging
        if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
            imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        VKM_CHECK_RESULT(
            vkCreateImage(t_device->logicalDevice, &imageCreateInfo, nullptr, &m_image));

        vkGetImageMemoryRequirements(t_device->logicalDevice, m_image, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;

        memAllocInfo.memoryTypeIndex
            = t_device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VKM_CHECK_RESULT(
            vkAllocateMemory(t_device->logicalDevice, &memAllocInfo, nullptr, &m_deviceMemory));
        VKM_CHECK_RESULT(vkBindImageMemory(t_device->logicalDevice, m_image, m_deviceMemory, 0));

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
        subresourceRange.layerCount = 1;

        // Image barrier for optimal image (target)
        // Optimal image will be used as destination for the copy
        tools::setImageLayout(copyCmd,
            m_image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);

        // Copy mip levels from staging buffer
        vkCmdCopyBufferToImage(copyCmd,
            stagingBuffer,
            m_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(bufferCopyRegions.size()),
            bufferCopyRegions.data());

        // Change texture image layout to shader read after all mip levels have been
        // copied
        this->m_imageLayout = t_imageLayout;
        tools::setImageLayout(copyCmd,
            m_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            t_imageLayout,
            subresourceRange);

        t_device->flushCommandBuffer(copyCmd, t_copyQueue);

        // Clean up staging resources
        vkFreeMemory(t_device->logicalDevice, stagingMemory, nullptr);
        vkDestroyBuffer(t_device->logicalDevice, stagingBuffer, nullptr);
    } else {
        // Prefer using optimal tiling, as linear tiling
        // may support only a small set of features
        // depending on implementation (e.g. no mip maps, only one layer, etc.)

        // Check if this support is supported for linear tiling
        assert(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

        VkImage mappableImage;
        VkDeviceMemory mappableMemory;

        VkImageCreateInfo imageCreateInfo = {};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = t_format;
        imageCreateInfo.extent = { m_width, m_height, 1 };
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
        imageCreateInfo.usage = t_imageUsageFlags;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        // Load mip map level 0 to linear tiling image
        VKM_CHECK_RESULT(
            vkCreateImage(t_device->logicalDevice, &imageCreateInfo, nullptr, &mappableImage));

        // Get memory requirements for this image
        // like size and alignment
        vkGetImageMemoryRequirements(t_device->logicalDevice, mappableImage, &memReqs);
        // Set memory allocation size to required memory size
        memAllocInfo.allocationSize = memReqs.size;

        // Get memory type that can be mapped to host memory
        memAllocInfo.memoryTypeIndex = t_device->getMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        // Allocate host memory
        VKM_CHECK_RESULT(
            vkAllocateMemory(t_device->logicalDevice, &memAllocInfo, nullptr, &mappableMemory));

        // Bind allocated image for use
        VKM_CHECK_RESULT(
            vkBindImageMemory(t_device->logicalDevice, mappableImage, mappableMemory, 0));

        // Get sub resource layout
        // Mip map count, array layer, etc.
        VkImageSubresource subRes = {};
        subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subRes.mipLevel = 0;

        VkSubresourceLayout subResLayout;
        void* data;

        // Get sub resources layout
        // Includes row pitch, size offsets, etc.
        vkGetImageSubresourceLayout(t_device->logicalDevice, mappableImage, &subRes, &subResLayout);

        // Map image memory
        VKM_CHECK_RESULT(
            vkMapMemory(t_device->logicalDevice, mappableMemory, 0, memReqs.size, 0, &data));

        // Copy image data into memory
        memcpy(data, FreeImage_GetBits(t_fibitmap), memReqs.size);

        vkUnmapMemory(t_device->logicalDevice, mappableMemory);

        // Linear tiled images don't need to be staged
        // and can be directly used as textures
        m_image = mappableImage;
        m_deviceMemory = mappableMemory;
        this->m_imageLayout = t_imageLayout;

        // Setup image memory barrier
        tools::setImageLayout(copyCmd,
            m_image,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            t_imageLayout);

        t_device->flushCommandBuffer(copyCmd, t_copyQueue);
    }

    // Create a defaultsampler
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    // Max level-of-detail should match mip level count
    samplerCreateInfo.maxLod = 0.0f;
    // Only enable anisotropic filtering if enabled on the devicec
    samplerCreateInfo.maxAnisotropy = t_device->enabledFeatures.samplerAnisotropy
        ? t_device->properties.limits.maxSamplerAnisotropy
        : 1.0f;
    samplerCreateInfo.anisotropyEnable = t_device->enabledFeatures.samplerAnisotropy;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VKM_CHECK_RESULT(
        vkCreateSampler(t_device->logicalDevice, &samplerCreateInfo, nullptr, &m_sampler));

    // Create image view
    // Textures are not directly accessed by the shaders and
    // are abstracted by image views containing additional
    // information and sub resource ranges
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = t_format;
    viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R,
        VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_A };
    viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    // Linear tiling usually won't support mip maps
    // Only set mip map count if optimal tiling is used
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.image = m_image;
    VKM_CHECK_RESULT(vkCreateImageView(t_device->logicalDevice, &viewCreateInfo, nullptr, &m_view));

    // Update descriptor image info member that can be used for setting up
    // descriptor sets
    updateDescriptor();
}

void VulkanTexture2D::fromNothing(VkFormat t_format, uint32_t t_texWidth, uint32_t t_texHeight,
    Device* t_device, VkQueue t_copyQueue, VkFilter t_filter, VkImageUsageFlags t_imageUsageFlags,
    VkImageLayout t_imageLayout, VkImageAspectFlags t_aspectFlags)
{
    this->m_device = t_device;
    m_width = t_texWidth;
    m_height = t_texHeight;
    this->m_imageLayout = t_imageLayout;

    VkImageCreateInfo image = {};
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = t_format;
    image.extent.width = t_texWidth;
    image.extent.height = t_texHeight;
    image.extent.depth = 1;
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = t_imageUsageFlags;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VKM_CHECK_RESULT(vkCreateImage(m_device->logicalDevice, &image, nullptr, &m_image));

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_device->logicalDevice, m_image, &memReqs);
    VkMemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memReqs.size;
    memoryAllocateInfo.memoryTypeIndex
        = m_device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VKM_CHECK_RESULT(
        vkAllocateMemory(m_device->logicalDevice, &memoryAllocateInfo, nullptr, &m_deviceMemory));
    VKM_CHECK_RESULT(vkBindImageMemory(m_device->logicalDevice, m_image, m_deviceMemory, 0));

    VkImageViewCreateInfo colorImageView = {};
    colorImageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    colorImageView.format = t_format;
    colorImageView.components = { VK_COMPONENT_SWIZZLE_R,
        VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_A };
    colorImageView.subresourceRange = {};
    colorImageView.subresourceRange.aspectMask = t_aspectFlags;
    colorImageView.subresourceRange.baseMipLevel = 0;
    colorImageView.subresourceRange.levelCount = 1;
    colorImageView.subresourceRange.baseArrayLayer = 0;
    colorImageView.subresourceRange.layerCount = 1;
    colorImageView.image = m_image;
    VKM_CHECK_RESULT(vkCreateImageView(m_device->logicalDevice, &colorImageView, nullptr, &m_view))

    // Create a defaultsampler
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = t_filter;
    samplerCreateInfo.minFilter = t_filter;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    // Max level-of-detail should match mip level count
    samplerCreateInfo.maxLod = 0.0f;
    // Only enable anisotropic filtering if enabled on the devicec
    samplerCreateInfo.maxAnisotropy = m_device->enabledFeatures.samplerAnisotropy
        ? m_device->properties.limits.maxSamplerAnisotropy
        : 1.0f;
    samplerCreateInfo.anisotropyEnable = m_device->enabledFeatures.samplerAnisotropy;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VKM_CHECK_RESULT(
        vkCreateSampler(m_device->logicalDevice, &samplerCreateInfo, nullptr, &m_sampler));

    VkCommandBuffer cmdBuffer
        = m_device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    tools::setImageLayout(cmdBuffer,
        m_image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        m_imageLayout,
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
    m_device->flushCommandBuffer(cmdBuffer, t_copyQueue);

    // Update descriptor image info member that can be used for setting up
    // descriptor sets
    updateDescriptor();
}

void VulkanTexture2D::depthAttachment(VkFormat t_format, uint32_t t_texWidth, uint32_t t_texHeight,
    Device* t_device, VkQueue t_copyQueue, VkSampleCountFlagBits t_samples, VkImageUsageFlags t_imageUsageFlags)
{
    this->m_device = t_device;
    m_width = t_texWidth;
    m_height = t_texHeight;
    this->m_imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkImageCreateInfo image = {};
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = t_format;
    image.extent.width = t_texWidth;
    image.extent.height = t_texHeight;
    image.extent.depth = 1;
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = t_samples;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | t_imageUsageFlags;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VKM_CHECK_RESULT(vkCreateImage(m_device->logicalDevice, &image, nullptr, &m_image));

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_device->logicalDevice, m_image, &memReqs);
    VkMemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memReqs.size;
    memoryAllocateInfo.memoryTypeIndex
        = m_device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VKM_CHECK_RESULT(
        vkAllocateMemory(m_device->logicalDevice, &memoryAllocateInfo, nullptr, &m_deviceMemory));
    VKM_CHECK_RESULT(vkBindImageMemory(m_device->logicalDevice, m_image, m_deviceMemory, 0));

    VkImageViewCreateInfo colorImageView = {};
    colorImageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    colorImageView.format = t_format;
    colorImageView.subresourceRange = {};
    colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    colorImageView.subresourceRange.baseMipLevel = 0;
    colorImageView.subresourceRange.levelCount = 1;
    colorImageView.subresourceRange.baseArrayLayer = 0;
    colorImageView.subresourceRange.layerCount = 1;
    colorImageView.image = m_image;
    VKM_CHECK_RESULT(vkCreateImageView(m_device->logicalDevice, &colorImageView, nullptr, &m_view));

    // Create a defaultsampler
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    // Max level-of-detail should match mip level count
    samplerCreateInfo.maxLod = 0.0f;
    samplerCreateInfo.maxAnisotropy = 1.0f;
    samplerCreateInfo.anisotropyEnable = VK_FALSE;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VKM_CHECK_RESULT(
        vkCreateSampler(m_device->logicalDevice, &samplerCreateInfo, nullptr, &m_sampler));

    VkCommandBuffer cmdBuffer
        = m_device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    tools::setImageLayout(cmdBuffer,
        m_image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        m_imageLayout,
        { colorImageView.subresourceRange.aspectMask, 0, 1, 0, 1 });
    m_device->flushCommandBuffer(cmdBuffer, t_copyQueue);

    // Update descriptor image info member that can be used for setting up
    // descriptor sets
    updateDescriptor();
}

void VulkanTexture2D::colorAttachment(VkFormat t_format, uint32_t t_texWidth, uint32_t t_texHeight,
    Device* t_device, VkQueue t_copyQueue, VkSampleCountFlagBits t_samples, VkImageUsageFlags t_imageUsageFlags)
{
    this->m_device = t_device;
    m_width = t_texWidth;
    m_height = t_texHeight;
    this->m_imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkImageCreateInfo image = {};
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = t_format;
    image.extent.width = t_texWidth;
    image.extent.height = t_texHeight;
    image.extent.depth = 1;
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = t_samples;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | t_imageUsageFlags;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VKM_CHECK_RESULT(vkCreateImage(m_device->logicalDevice, &image, nullptr, &m_image));

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_device->logicalDevice, m_image, &memReqs);
    VkMemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memReqs.size;
    memoryAllocateInfo.memoryTypeIndex
        = m_device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VKM_CHECK_RESULT(
        vkAllocateMemory(m_device->logicalDevice, &memoryAllocateInfo, nullptr, &m_deviceMemory));
    VKM_CHECK_RESULT(vkBindImageMemory(m_device->logicalDevice, m_image, m_deviceMemory, 0));

    VkImageViewCreateInfo colorImageView = {};
    colorImageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    colorImageView.format = t_format;
    colorImageView.subresourceRange = {};
    colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorImageView.subresourceRange.baseMipLevel = 0;
    colorImageView.subresourceRange.levelCount = 1;
    colorImageView.subresourceRange.baseArrayLayer = 0;
    colorImageView.subresourceRange.layerCount = 1;
    colorImageView.image = m_image;
    VKM_CHECK_RESULT(vkCreateImageView(m_device->logicalDevice, &colorImageView, nullptr, &m_view));

    // Create a defaultsampler
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    // Max level-of-detail should match mip level count
    samplerCreateInfo.maxLod = 1.0f;
    // Only enable anisotropic filtering if enabled on the devicec
    samplerCreateInfo.maxAnisotropy = 1.0f;
    samplerCreateInfo.anisotropyEnable = m_device->enabledFeatures.samplerAnisotropy;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VKM_CHECK_RESULT(
        vkCreateSampler(m_device->logicalDevice, &samplerCreateInfo, nullptr, &m_sampler));

    VkCommandBuffer cmdBuffer
        = m_device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    tools::setImageLayout(cmdBuffer,
        m_image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        m_imageLayout,
        { colorImageView.subresourceRange.aspectMask, 0, 1, 0, 1 });
    m_device->flushCommandBuffer(cmdBuffer, t_copyQueue);

    // Update descriptor image info member that can be used for setting up
    // descriptor sets
    updateDescriptor();
}

VulkanTexture2D::VulkanTexture2D()
    : Texture()
{
}

VulkanTexture2D::~VulkanTexture2D() = default;
