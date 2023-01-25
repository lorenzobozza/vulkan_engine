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

#include "libtiff/tiffio.h"

// std
#include <iostream>
#include <chrono>

Texture::Texture(Device &dev, Image &image, std::string filePath, VkFormat format)
    : device{dev}, image{image}, textureFilePath{filePath}, viewType{VK_IMAGE_VIEW_TYPE_2D}, format{format} {
    loadTexture();
    TIFFSetWarningHandler(NULL);
}

Texture::Texture(Device &dev, Image &image, std::string filePath, VkImageViewType viewType, VkFormat format)
    : device{dev}, image{image}, textureFilePath{filePath}, viewType{viewType}, format{format} {
    loadTexture();
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    TIFFSetWarningHandler(NULL);
}

Texture::~Texture() {
    vkDestroySampler(device.device(), textureSampler, nullptr);
    vkDestroyImageView(device.device(), textureImageView, nullptr);
    vkDestroyImage(device.device(), textureImage, nullptr);
    vkFreeMemory(device.device(), textureImageMemory, nullptr);
}

void Texture::moveBuffer() {
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
}

VkDescriptorImageInfo Texture::descriptorInfo() {
    return VkDescriptorImageInfo {
        textureSampler,
        textureImageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
}

void Texture::loadTexture() {
    uint8_t depth;
    size_t bitsPerPixel;
    switch (format) {
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
            bitsPerPixel = depth * sizeof(float32_t);
            #else
            bitsPerPixel = depth * sizeof(float);
            #endif
            break;
            
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            depth = STBI_rgb_alpha;
            #ifdef __aarch64__
            bitsPerPixel = depth * sizeof(float16_t);
            #else
            bitsPerPixel = depth * (sizeof(float) / 2);
            #endif
            break;
            
        default:
            depth = STBI_rgb_alpha;
            bitsPerPixel = depth * sizeof(int8_t);
            break;
    }
    
    bool libtiff = false;
    int texWidth, texHeight, texChannels;
    void* pixels;

    auto fileExt = textureFilePath.substr(textureFilePath.size() - 4, textureFilePath.size() - 1);
    if (fileExt == ".tif" || fileExt == "tiff") {
        TIFF* tif = TIFFOpen(textureFilePath.c_str(), "r");
        TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &texWidth);
        TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &texHeight);
        
        pixels = _TIFFmalloc(texWidth * texHeight * sizeof (uint32_t));
        if (pixels != NULL) {
            if (TIFFReadRGBAImage(tif, texWidth, texHeight, (uint32_t*)pixels, 0)) {
                libtiff = true;
            } else {
                _TIFFfree(pixels);
            }
        }
        TIFFClose(tif);
    } else {
        if(stbi_is_hdr(textureFilePath.c_str())) {
            float* data = stbi_loadf(textureFilePath.c_str(), &texWidth, &texHeight, &texChannels, depth);
            pixels = (void*)data;
        } else {
            stbi_uc* data = stbi_load(textureFilePath.c_str(), &texWidth, &texHeight, &texChannels, depth);
            pixels = (void*)data;
        }
    }

    VkDeviceSize imageSize = texWidth * texHeight * bitsPerPixel;
    
    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }
    
    stagingBuffer = std::make_unique<Buffer>(
        device,
        imageSize,
        1,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    
    stagingBuffer->map();
    stagingBuffer->writeToBuffer(pixels);
    
    if (libtiff) {
        _TIFFfree(pixels);
    } else {
        stbi_image_free(pixels);
    }
    
    _w = texWidth;
    _h = texHeight;
    
    mipLevels = std::floor(std::log2(std::max(_w, _h))) + 1;
}

void Texture::createTextureImage() {

    image.createImage(_w, _h, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory, 1, mipLevels);
    
    auto commandBuffer = image.beginSingleTimeCommands();
    
    // Load mip 0 from buffer
    image.transitionImageLayout(commandBuffer, textureImage, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, mipLevels);
    image.copyBufferToImage(commandBuffer, stagingBuffer->getBuffer(), textureImage, static_cast<uint32_t>(_w), static_cast<uint32_t>(_h));
    
    int mipWidth = _w;
    int mipHeight = _h;
    
    VkFormatProperties prop;
    vkGetPhysicalDeviceFormatProperties(device.getPhysicalDevice(), format, &prop);
    if (!(prop.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("Linear filtering not supported on this image format");
    }
    
    // Compute mips
    for (int i = 1; i < mipLevels; i++) {
        // Prepare mip i - 1 to be read
        image.transitionImageLayout(commandBuffer, textureImage, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1, 1, i - 1);
        // Prepare mip i to be written
        image.transitionImageLayout(commandBuffer, textureImage, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 1, i);
        
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
        vkCmdBlitImage(commandBuffer, textureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
        
        // Set mip i - 1 to final shader read only layout
        image.transitionImageLayout(commandBuffer, textureImage, format, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, i - 1);
        
        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }
    
    // Set last mip to final shader read only layout
    image.transitionImageLayout(commandBuffer, textureImage, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1, mipLevels - 1);
    
    image.endSingleTimeCommands(commandBuffer);
    
    stagingBuffer = nullptr;
}

void Texture::createTextureImageView() {
    textureImageView = image.createImageView(textureImage, viewType, format, 1, mipLevels);
}

void Texture::createTextureSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = device.properties.limits.maxSamplerAnisotropy / 4;
    
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(mipLevels);
    
    if (vkCreateSampler(device.device(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}
