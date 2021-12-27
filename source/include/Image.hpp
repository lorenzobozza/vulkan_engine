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

class Image {
public:
    Image(Device &dev, uint32_t layerCount = 1);
    ~Image();
    
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    void transitionImageLayout(VkCommandBuffer &commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkCommandBuffer &commandBuffer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    VkImageView createImageView(VkImage image, VkFormat format);
    
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    
private:
    Device &device;
    
    VkCommandPool commandPool;
    
    uint32_t layerCount;
};

#endif /* Image_hpp */
