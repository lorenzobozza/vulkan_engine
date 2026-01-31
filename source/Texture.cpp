//
//  Texture.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 09/12/21.
//

#include "include/Texture.hpp"

// lib
#define STB_IMAGE_IMPLEMENTATION
#include <stb-master/stb_image.h>

#define TINYEXR_IMPLEMENTATION
#include <tinyexr/tinyexr.h>

// std
#include <chrono>

Texture::Texture(const Device& dev, const Image& image, std::string filePath, bool mipMapping, VkFormat format)
: m_Device{dev}, m_Image{image}, m_TextureFilePath{filePath}, m_MipMapping{mipMapping}, m_ViewType{VK_IMAGE_VIEW_TYPE_2D}, m_Format{format} {
    loadTexture();
    createDefaultTextureSampler();
    createTextureImage();
    createTextureImageView();
}

Texture::Texture(const Device& dev, const Image& image, void* data, uint32_t texWidth, uint32_t texHeight, uint8_t depth, bool mipMapping, VkFormat format, VkSamplerCreateInfo *samplerInfo)
: m_Device{dev}, m_Image{image}, m_MipMapping{mipMapping}, m_ViewType{VK_IMAGE_VIEW_TYPE_2D}, m_Format{format} {

    VkDeviceSize imageSize = texWidth * texHeight * depth;
    if (m_Format > VK_FORMAT_A8B8G8R8_SRGB_PACK32) {
        imageSize *= sizeof(uint16_t);
    } else {
        imageSize *= sizeof(uint8_t);
    }
    
    m_StagingBuffer = std::make_unique<Buffer>(m_Device, imageSize, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_StagingBuffer->map();
    m_StagingBuffer->writeToBuffer(data);
    
    m_Width = texWidth;
    m_Height = texHeight;
    
    if (mipMapping) {
        m_MipLevels = (int)std::floor(std::log2(std::max(m_Width, m_Height))) + 1;
        m_MipLevels = (m_MipLevels > 6) ? 6 : m_MipLevels;
    }
    else {
        m_MipLevels = 1;
    }
    
    samplerInfo->sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    
    float maxSamplerAnisotropy = m_Device.getPhysicalDeviceProp().limits.maxSamplerAnisotropy;
    
    samplerInfo->anisotropyEnable = m_MipLevels > 1 ? VK_TRUE : VK_FALSE;
    samplerInfo->maxAnisotropy = (maxSamplerAnisotropy > 8.0f) ? 8.0f : maxSamplerAnisotropy;
    
    samplerInfo->borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    samplerInfo->unnormalizedCoordinates = VK_FALSE;
    
    samplerInfo->compareEnable = VK_FALSE;
    samplerInfo->compareOp = VK_COMPARE_OP_ALWAYS;
    
    samplerInfo->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo->mipLodBias = 0.0f;
    samplerInfo->minLod = 0.0f;
    samplerInfo->maxLod = static_cast<float>(m_MipLevels);
    
    if (vkCreateSampler(m_Device.device(), samplerInfo, nullptr, &m_TextureSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
    
    createTextureImage();
    createTextureImageView();
}

Texture::~Texture() {
    vkDestroySampler(m_Device.device(), m_TextureSampler, nullptr);
    vkDestroyImageView(m_Device.device(), m_TextureImageView, nullptr);
    vkDestroyImage(m_Device.device(), m_TextureImage, nullptr);
    vkFreeMemory(m_Device.device(), m_TextureImageMemory, nullptr);
}

VkDescriptorImageInfo Texture::descriptorInfo(void) const {
    return VkDescriptorImageInfo {
        m_TextureSampler,
        m_TextureImageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
}

void Texture::loadTexture(void) {
    uint8_t depth;
    size_t bitsPerPixel;
    switch (m_Format) {
    case VK_FORMAT_R8G8B8A8_SRGB:
        depth = STBI_rgb_alpha;
        bitsPerPixel = depth * sizeof(int8_t);
        break;
        
    case VK_FORMAT_R8G8B8A8_UNORM:
        depth = STBI_rgb_alpha;
        bitsPerPixel = depth * sizeof(int8_t);
        break;
        
    case VK_FORMAT_R8G8B8_UNORM:
        depth = STBI_rgb;
        bitsPerPixel = depth * sizeof(int8_t);
        break;
        
    case VK_FORMAT_R8G8_UNORM:
        depth = STBI_grey_alpha;
        bitsPerPixel = depth * sizeof(int8_t);
        break;
        
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        depth = STBI_rgb_alpha;
#ifdef __aarch64__
        bitsPerPixel = depth * sizeof(uint32_t);
#else
        bitsPerPixel = depth * sizeof(float);
#endif
        break;
        
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        depth = STBI_rgb_alpha;
#ifdef __aarch64__
        bitsPerPixel = depth * sizeof(uint16_t);
#else
        bitsPerPixel = depth * (sizeof(float) / 2);
#endif
        break;
        
    default:
        depth = STBI_rgb_alpha;
        bitsPerPixel = depth * sizeof(int8_t);
        break;
    }
    
    int texWidth, texHeight, texChannels;
    void* pixels;
    
    auto fileExt = m_TextureFilePath.substr(m_TextureFilePath.size() - 4, m_TextureFilePath.size() - 1);
    
    if (fileExt == ".exr") {
        float* data;
        int ret = LoadEXR(&data, &texWidth, &texHeight, m_TextureFilePath.c_str(), nullptr);
        pixels = (void*)data;
    } else {
        if(stbi_is_hdr(m_TextureFilePath.c_str())) {
            float* data = stbi_loadf(m_TextureFilePath.c_str(), &texWidth, &texHeight, &texChannels, depth);
            pixels = (void*)data;
        } else {
            stbi_uc* data = stbi_load(m_TextureFilePath.c_str(), &texWidth, &texHeight, &texChannels, depth);
            pixels = (void*)data;
        }
    }
    
    VkDeviceSize imageSize = texWidth * texHeight * bitsPerPixel;
    
    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }
    
    m_StagingBuffer = std::make_unique<Buffer>(m_Device, imageSize, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_StagingBuffer->map();
    m_StagingBuffer->writeToBuffer(pixels);
    
    free(pixels);
    
    m_Width = texWidth;
    m_Height = texHeight;
    
    if (m_MipMapping) {
        m_MipLevels = (int)std::floor(std::log2(std::max(m_Width, m_Height))) + 1;
    }
    else {
        m_MipLevels = 1;
    }
    
}

void Texture::createTextureImage(void) {
    m_Image.createImage(m_Width, m_Height, m_Format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_TextureImage, m_TextureImageMemory, 1, m_MipLevels);
    
    auto commandBuffer = m_Image.beginSingleTimeCommands();
    
    // Load mip 0 from staging buffer
    m_Image.transitionImageLayout(commandBuffer, m_TextureImage, m_Format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, m_MipLevels);
    m_Image.copyBufferToImage(commandBuffer, m_StagingBuffer->getBuffer(), m_TextureImage, static_cast<uint32_t>(m_Width), static_cast<uint32_t>(m_Height));
    
    int mipWidth = m_Width;
    int mipHeight = m_Height;
    
    VkFormatProperties prop;
    vkGetPhysicalDeviceFormatProperties(m_Device.getPhysicalDevice(), m_Format, &prop);
    if (!(prop.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("Linear filtering not supported on this image format");
    }
    
    // Compute mips
    for (int i = 1; i < m_MipLevels; i++) {
        // Prepare mip i - 1 to be read
        m_Image.transitionImageLayout(commandBuffer, m_TextureImage, m_Format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1, 1, i - 1);
        // Prepare mip i to be written
        m_Image.transitionImageLayout(commandBuffer, m_TextureImage, m_Format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 1, i);
        
        VkImageBlit blit{};
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;
        
        // Perform texture filtering from mip i - 1 to mip i
        vkCmdBlitImage(commandBuffer, m_TextureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_TextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
        
        // Set mip i - 1 to final shader read only layout
        m_Image.transitionImageLayout(commandBuffer, m_TextureImage, m_Format, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, i - 1);
        
        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }
    
    // Set last mip to final shader read only layout
    m_Image.transitionImageLayout(commandBuffer, m_TextureImage, m_Format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, m_MipLevels - 1);
    
    m_Image.endSingleTimeCommands(commandBuffer);
    
    m_StagingBuffer = nullptr;
}

void Texture::createTextureImageView(void) {
    m_TextureImageView = m_Image.createImageView(m_TextureImage, m_ViewType, m_Format, 1, m_MipLevels);
}

void Texture::createDefaultTextureSampler(void) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    
    float maxSamplerAnisotropy = m_Device.getPhysicalDeviceProp().limits.maxSamplerAnisotropy;
    
    samplerInfo.anisotropyEnable = m_MipLevels > 1 ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy = (maxSamplerAnisotropy > 8.0f) ? 8.0f : maxSamplerAnisotropy;
    
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(m_MipLevels);
    
    if (vkCreateSampler(m_Device.device(), &samplerInfo, nullptr, &m_TextureSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}
