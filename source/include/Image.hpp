//
//  Image.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 05/12/21.
//

#ifndef Image_hpp
#define Image_hpp

#include "Device.hpp"
#include "Buffer.hpp"

//std
#include <memory>

class Image {
public:
    Image(Device &dev);
    ~Image();
    
    void createImage(
        uint32_t width, uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory,
        uint32_t layerCount = 1,
        uint32_t levelCount = 1,
        VkImageCreateFlags flags = 0);
    
    void transitionImageLayout(
        VkCommandBuffer &commandBuffer,
        VkImage image, VkFormat format,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        uint32_t layerCount = 1,
        uint32_t levelCount = 1,
        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);
    
    void copyBufferToImage(
        VkCommandBuffer &commandBuffer,
        VkBuffer buffer,
        VkImage image,
        uint32_t width, uint32_t height,
        uint32_t layerCount = 1);
    
    VkImageView createImageView(
        VkImage image,
        VkImageViewType viewType,
        VkFormat format,
        uint32_t layerCount = 1,
        uint32_t levelCount = 1,
        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);
    
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    
private:
    Device &device;
    
    VkCommandPool commandPool;
};

#endif /* Image_hpp */
