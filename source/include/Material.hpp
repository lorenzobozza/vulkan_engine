//
//  Material.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 12/09/25.
//

#pragma once

#include <glm/glm.hpp>
#include "Texture.hpp"

#define COLOR_TEXTURE       0x1
#define NORMAL_TEXTURE      0x2
#define OCCLUSION_TEXTURE   0x4
#define ROUGH_METAL_TEXTURE 0x8

#define COLOR_UV            0x10
#define NORMAL_UV           0x20
#define OCCLUSION_UV        0x40
#define ROUGH_METAL_UV      0x80

class Material {
    
    // Id == 0 default texture
    size_t colorTextureId{0};
    size_t normalTextureId{0};
    size_t occlusionTextureId{0};
    size_t roughMetalTextureId{0};
    
    unsigned int textureBitmap{0};
    
    std::vector<std::unique_ptr<Texture>> *pTexs{nullptr};

    
public:
    Material(std::vector<std::unique_ptr<Texture>> *pTexs) : pTexs{pTexs} {}
    Material() {}
    
    ~Material() {}
    
    glm::vec4 color{1.f};
    float metalness{0.f};
    float roughness{1.f};
    
    enum AlphaMode{ ALPHAMODE_OPAQUE = 0, ALPHAMODE_MASK, ALPHAMODE_BLEND };
    AlphaMode alphaMode = ALPHAMODE_OPAQUE;
    float alphaCutoff{0.5};
    
    void setColorTexture(size_t id) { textureBitmap |= COLOR_TEXTURE; colorTextureId = id; }
    void setNormalTexture(size_t id) { textureBitmap |= NORMAL_TEXTURE; normalTextureId = id; }
    void setOcclusionTexture(size_t id) { textureBitmap |= OCCLUSION_TEXTURE; occlusionTextureId = id; }
    void setRoughMetalTexture(size_t id) { textureBitmap |= ROUGH_METAL_TEXTURE; roughMetalTextureId = id; }
    
    void setColorTexCoordSet(int set) { textureBitmap |= (set == 1) ? COLOR_UV : 0; }
    void setNormalTexCoordSet(int set) { textureBitmap |= (set == 1) ? NORMAL_UV : 0; }
    void setOcclusionTexCoordSet(int set) { textureBitmap |= (set == 1) ? OCCLUSION_UV : 0; }
    void setMetalRoughTexCoordSet(int set) { textureBitmap |= (set == 1) ? ROUGH_METAL_UV : 0; }
    
    VkDescriptorImageInfo getColorTextureIF(void) { return pTexs->at(colorTextureId)->descriptorInfo(); }
    VkDescriptorImageInfo getNormalTextureIF(void) { return pTexs->at(normalTextureId)->descriptorInfo(); }
    VkDescriptorImageInfo getOcclusionTextureIF(void) { return pTexs->at(occlusionTextureId)->descriptorInfo(); }
    VkDescriptorImageInfo getMetalRoughTextureIF(void) { return pTexs->at(roughMetalTextureId)->descriptorInfo(); }
    
    unsigned int getTextureBitmap(void) { return textureBitmap; }
};
