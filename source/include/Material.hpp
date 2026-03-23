//
//  Material.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 12/09/25.
//

#ifndef Material_hpp
#define Material_hpp

#include "Texture.hpp"

#include <glm/glm.hpp>

#include <atomic>

#define COLOR_TEXTURE       0x1
#define NORMAL_TEXTURE      0x2
#define ROUGH_METAL_TEXTURE 0x4
#define COMBO_ARM_TEXTURE   0x8
#define SPLIT_AO_TEXTURE    0x10

#define COLOR_UV            0x100
#define NORMAL_UV           0x200
#define ROUGH_METAL_UV      0x400
#define OCCLUSION_UV        0x800


class Material {
public:
    Material() = default;
    ~Material() = default;
    
    glm::vec4 color{1.f};
    float metalness{0.f};
    float roughness{1.f};
    
    enum AlphaMode{ ALPHAMODE_OPAQUE = 0, ALPHAMODE_MASK, ALPHAMODE_BLEND };
    AlphaMode alphaMode = ALPHAMODE_OPAQUE;
    float alphaCutoff{0.5};
    
    float f0{.04f};
    float coatWeight{0.f};
    float coatRoughness{.04f};
    float anisoStrength{0.f};
    float anisoRotation{0.f};
    
    void setColorTexture(size_t id) { m_TextureBitmap |= COLOR_TEXTURE; m_ColorTextureId = id; }
    void setNormalTexture(size_t id) { m_TextureBitmap |= NORMAL_TEXTURE; m_NormalTextureId = id; }
    void setRoughMetalTexture(size_t id) { m_TextureBitmap |= ROUGH_METAL_TEXTURE; m_RoughMetalTextureId = id; }
    void setARMTexture(size_t id) { m_TextureBitmap |= COMBO_ARM_TEXTURE; m_OcclusionTextureId = id; }
    void setOcclusionTexture(size_t id) { m_TextureBitmap |= SPLIT_AO_TEXTURE; m_OcclusionTextureId = id; }
    
    void setColorTexCoordSet(int set) { m_TextureBitmap |= (set == 1) ? COLOR_UV : 0; }
    void setNormalTexCoordSet(int set) { m_TextureBitmap |= (set == 1) ? NORMAL_UV : 0; }
    void setOcclusionTexCoordSet(int set) { m_TextureBitmap |= (set == 1) ? OCCLUSION_UV : 0; }
    void setMetalRoughTexCoordSet(int set) { m_TextureBitmap |= (set == 1) ? ROUGH_METAL_UV : 0; }
    
    using Textures = std::vector<std::unique_ptr<const Texture>>;
    VkDescriptorImageInfo getColorDescriptor(Textures& texVec) const { return texVec[m_ColorTextureId]->descriptorInfo(); }
    VkDescriptorImageInfo getNormalDescriptor(Textures& texVec) const { return texVec[m_NormalTextureId]->descriptorInfo(); }
    VkDescriptorImageInfo getOcclusionDescriptor(Textures& texVec) const { return texVec[m_OcclusionTextureId]->descriptorInfo(); }
    VkDescriptorImageInfo getMetalRoughDescriptor(Textures& texVec) const { return texVec[m_RoughMetalTextureId]->descriptorInfo(); }
    
    unsigned int getTextureBitmap(void) { return m_TextureBitmap; }
    
private:
    size_t m_ColorTextureId{0};
    size_t m_NormalTextureId{0};
    size_t m_OcclusionTextureId{0};
    size_t m_RoughMetalTextureId{0};
    
    uint32_t m_TextureBitmap{0};
};

#endif
