//
//  UI.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 15/12/22.
//

#ifndef UI_hpp
#define UI_hpp

#include "imgui.h"
#include "include/Device.hpp"
#include "include/Buffer.hpp"
#include "include/Pipeline.hpp"
#include "include/Descriptors.hpp"
#include "include/Image.hpp"

class UI {
public:
    UI(Device &device);
    ~UI();
    
    struct PushConstBlock {
		glm::vec2 scale;
		glm::vec2 translate;
	} pushConstBlock;
    
    void createPipeline(VkDescriptorSetLayout descriptorSetLayout, VkRenderPass renderPass, std::string dynamicShaderPath);
    void draw(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet);
    void newFrame();
    void updateBuffers();
    
    
    VkDescriptorImageInfo loadFontTexture();

private:
    std::vector<char> readFile(const std::string &filepath);

    Device &device;
    Image vulkanImage{device};
    
    VkPipeline imguiPipeline;
    VkPipelineLayout imguiPipelineLayout;
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    
    Buffer vertexBuffer{device};
    Buffer indexBuffer{device};
    int vertexCount, indexCount;
    
    VkImage fontImage;
    VkDeviceMemory fontMem;
    VkImageView fontView;
    VkSampler fontSampler;
};

#endif /* UI_hpp */
