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
    
    Texture(Device &dev, Image &image, const char* filePath, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
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
    
    VkFormat format;
    
    const char* textureFilePath;
};

#endif /* Texture_hpp */
