//
//  FrameInfo.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 15/11/21.
//

#ifndef FrameInfo_hpp
#define FrameInfo_hpp

#include "Camera.hpp"
#include "Primitive.hpp"
#include "Material.hpp"

//lib
#include <vulkan/vulkan.h>

struct FrameInfo {
    int frameIndex;
    float frameTime;
    VkCommandBuffer commandBuffer;
    Camera &camera;
    Primitive::Map &primitives;
    VkDescriptorSet mainDescriptorSet;
    std::unordered_map<std::string, Material> &materials;
    std::unordered_map<std::string, VkDescriptorSet> materialDescriptorSets;
};

struct FrameInfoNoMaterials {
    int frameIndex;
    float frameTime;
    VkCommandBuffer commandBuffer;
    Camera &camera;
    Primitive::Map &primitives;
    VkDescriptorSet mainDescriptorSet;
};

#endif /* FrameInfo_hpp */
