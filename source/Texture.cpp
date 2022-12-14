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

//#define TINYEXR_USE_MINIZ 0
//#define TINYEXR_USE_STB_ZLIB 1
//#define TINYEXR_IMPLEMENTATION
//#include <tinyexr/tinyexr.h>

Texture::Texture(Device &dev, Image &image, std::string filePath, VkFormat format)
    : device{dev}, image{image}, textureFilePath{filePath}, viewType{VK_IMAGE_VIEW_TYPE_2D}, format{format} {
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
}

Texture::Texture(Device &dev, Image &image, std::string filePath, VkImageViewType viewType, VkFormat format)
    : device{dev}, image{image}, textureFilePath{filePath}, viewType{viewType}, format{format} {
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
}

Texture::~Texture() {
    vkDestroySampler(device.device(), textureSampler, nullptr);
    vkDestroyImageView(device.device(), textureImageView, nullptr);
    vkDestroyImage(device.device(), textureImage, nullptr);
    vkFreeMemory(device.device(), textureImageMemory, nullptr);
}

VkDescriptorImageInfo Texture::descriptorInfo() {
    return VkDescriptorImageInfo {
        textureSampler,
        textureImageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
}

void Texture::createTextureImage() {
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
            bitsPerPixel = depth * sizeof(float32_t);
            break;
            
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            depth = STBI_rgb_alpha;
            bitsPerPixel = depth * sizeof(float16_t);
            break;
            
        default:
            depth = STBI_rgb_alpha;
            bitsPerPixel = depth * sizeof(int8_t);
            break;
    }
    
    int texWidth, texHeight, texChannels;
    
    void* pixels;
    if(stbi_is_hdr(textureFilePath.c_str())) {
        float* data = stbi_loadf(textureFilePath.c_str(), &texWidth, &texHeight, &texChannels, depth);
        pixels = (void*)data;
    } else {
        stbi_uc* data = stbi_load(textureFilePath.c_str(), &texWidth, &texHeight, &texChannels, depth);
        pixels = (void*)data;
    }

    VkDeviceSize imageSize = texWidth * texHeight * bitsPerPixel;
    
    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }
    
    Buffer stagingBuffer{
        device,
        imageSize,
        1,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };
    
    stagingBuffer.map();
    stagingBuffer.writeToBuffer(pixels);
    
    stbi_image_free(pixels);
    
    image.createImage(texWidth, texHeight, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);
    
    auto commandBuffer = image.beginSingleTimeCommands();
    
    image.transitionImageLayout(commandBuffer, textureImage, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
    image.copyBufferToImage(commandBuffer, stagingBuffer.getBuffer(), textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    
    image.transitionImageLayout(commandBuffer, textureImage, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    image.endSingleTimeCommands(commandBuffer);
}

void Texture::createTextureImageView() {
    textureImageView = image.createImageView(textureImage, viewType, format);
}

void Texture::createTextureSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = device.properties.limits.maxSamplerAnisotropy;
    
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    
    if (vkCreateSampler(device.device(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}
