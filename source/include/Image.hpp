//
//  Image.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 05/12/21.
//

#ifndef Image_hpp
#define Image_hpp

#include "Device.hpp"

class Image {
public:
    Image(const Device& dev);
    ~Image();
    
    void createImage(uint32_t width, uint32_t height,
                     VkFormat format,
                     VkImageTiling tiling,
                     VkImageUsageFlags usage,
                     VkMemoryPropertyFlags properties,
                     VkImage& image,
                     VkDeviceMemory& imageMemory,
                     uint32_t layerCount = 1,
                     uint32_t levelCount = 1,
                     VkImageCreateFlags flags = 0) const;
    
    void transitionImageLayout(VkCommandBuffer &commandBuffer,
                               VkImage image, VkFormat format,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout,
                               uint32_t layerCount = 1,
                               uint32_t levelCount = 1,
                               uint32_t baseMipLevel = 0,
                               VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT) const;
    
    void copyBufferToImage(VkCommandBuffer &commandBuffer,
                           VkBuffer buffer,
                           VkImage image,
                           uint32_t width, uint32_t height,
                           uint32_t layerCount = 1,
                           uint32_t mipLevel = 0) const;
    
    VkImageView createImageView(VkImage image,
                                VkImageViewType viewType,
                                VkFormat format,
                                uint32_t layerCount = 1,
                                uint32_t levelCount = 1,
                                VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT) const;
    
    VkCommandBuffer beginSingleTimeCommands(void) const;
    void endSingleTimeCommands(VkCommandBuffer commandBuffer) const;
    
private:
    const Device& m_Device;
    VkCommandPool m_CommandPool;
};

#endif /* Image_hpp */
