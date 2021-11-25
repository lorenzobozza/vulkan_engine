//
//  FrameInfo.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 15/11/21.
//

#ifndef FrameInfo_hpp
#define FrameInfo_hpp

#include "Camera.hpp"
#include "SolidObject.hpp"

//lib
#include <vulkan/vulkan.h>

struct FrameInfo {
    int frameIndex;
    float frameTime;
    VkCommandBuffer commandBuffer;
    Camera &camera;
    VkDescriptorSet globalDescriptorSet;
    SolidObject::Map &solidObjects;
};

#endif /* FrameInfo_hpp */
