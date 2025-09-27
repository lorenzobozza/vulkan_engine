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
    std::unordered_map<std::string, VkDescriptorSet> globalDescriptorSet;
    Primitive::Map &primitives;
    std::unordered_map<std::string, Material> &materials;
};

#endif /* FrameInfo_hpp */
