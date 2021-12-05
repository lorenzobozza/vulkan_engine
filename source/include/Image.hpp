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
    Image(Device &dev, const char* path);
    ~Image();
    
    VkDescriptorImageInfo descriptorInfo();
    
private:
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void createTextureImage();
    
    VkImageView createImageView(VkImage image, VkFormat format);
    void createTextureImageView();
    
    void createTextureSampler();
    
    Device &device;
    
    VkImage textureImage{};
    VkDeviceMemory textureImageMemory{};
    VkImageView textureImageView{};
    VkSampler textureSampler{};
    
    const char* filePath;
};

#endif /* Image_hpp */
