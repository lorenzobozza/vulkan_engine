//
//  Texture.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 09/12/21.
//

#ifndef Texture_hpp
#define Texture_hpp

#include "Image.hpp"

class Texture {
public:
    
    Texture(Device &dev, Image &image, std::string filePath, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
    Texture(Device &dev, Image &image, std::string filePath, VkImageViewType viewType, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
    ~Texture();
    
    VkDescriptorImageInfo descriptorInfo();
    
private:
    void createTextureImage();
    void createTextureImageView();
    void createTextureSampler();
    
    Device &device;
    Image &image;
    
    VkImage textureImage{};
    VkDeviceMemory textureImageMemory{};
    VkImageView textureImageView{};
    VkSampler textureSampler{};
    
    VkImageViewType viewType;
    VkFormat format;
    
    std::string textureFilePath;
};

#endif /* Texture_hpp */
