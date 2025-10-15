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
    
    Texture(Device &dev, Image &image, std::string filePath, bool mipMapping, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
    Texture(Device &dev, Image &image, std::string filePath, bool mipMapping, VkImageViewType viewType, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
    Texture(Device &dev, Image &image, void* data, uint32_t texWidth, uint32_t texHeight, uint8_t depth, bool mipMapping, VkFormat format, VkSamplerCreateInfo *samplerInfo);
    ~Texture();
    
    void moveBuffer();
    VkDescriptorImageInfo descriptorInfo();
    
private:
    void loadTexture();
    void createTextureImage();
    void createTextureImageView();
    void createDefaultTextureSampler();
    
    Device &device;
    Image &image;
    
    std::unique_ptr<Buffer> stagingBuffer;
    int _w, _h;
    int mipLevels;
    
    bool mipMapping{false};
    
    VkImage textureImage{};
    VkDeviceMemory textureImageMemory{};
    VkImageView textureImageView{};
    VkSampler textureSampler{};
    
    VkImageViewType viewType;
    VkFormat format;
    
    std::string textureFilePath;
};

#endif /* Texture_hpp */
