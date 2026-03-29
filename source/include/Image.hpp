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
    
    void createImage(VkExtent3D extent,
                     VkImageType type, 
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
                           VkExtent3D extent,
                           uint32_t layerCount = 1,
                           uint32_t mipLevel = 0) const;
    
    void hostMemoryCopyTransition(const void* data, VkImage dstImage, VkImageAspectFlags aspect, VkExtent3D extent, VkImageLayout newLayout) const;
    
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
