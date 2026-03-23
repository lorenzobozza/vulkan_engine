//
//  Texture.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 09/12/21.
//

#ifndef Texture_hpp
#define Texture_hpp

#include "Image.hpp"
#include "Buffer.hpp"

#include <memory>

class Texture {
public:
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) = delete;
    Texture& operator=(Texture&&) = delete;
    
    Texture(const Device& dev, const Image& image, std::string filePath, bool mipMapping, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
    Texture(const Device& dev, const Image& image, void* data, uint32_t texWidth, uint32_t texHeight, uint8_t depth, bool mipMapping, VkFormat format, VkSamplerCreateInfo *samplerInfo);
    ~Texture();
    
    VkDescriptorImageInfo descriptorInfo(void) const;
    
private:
    void loadTexture(void);
    void createTextureImage(void);
    void createTextureImageView(void);
    void createDefaultTextureSampler(void);
    
    const Device& m_Device;
    const Image& m_Image;
    
    VkDeviceSize m_ImageSize;
    void* m_ImageData;
    bool m_IsStbiAllocated = false;
    
    uint32_t m_Width, m_Height;
    int m_MipLevels;
    
    bool m_MipMapping{false};
    
    VkImage m_TextureImage = VK_NULL_HANDLE;
    VkDeviceMemory m_TextureImageMemory = VK_NULL_HANDLE;
    VkImageView m_TextureImageView = VK_NULL_HANDLE;
    VkSampler m_TextureSampler = VK_NULL_HANDLE;
    
    VkImageViewType m_ViewType;
    VkFormat m_Format;
    std::string m_TextureFilePath;
};

#endif /* Texture_hpp */
