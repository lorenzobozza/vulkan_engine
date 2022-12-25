//
//  UI.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 15/12/22.
//

#ifndef UI_hpp
#define UI_hpp

#include "imgui.h"
#include "Application.hpp"
#include "Device.hpp"
#include "Buffer.hpp"
#include "Pipeline.hpp"
#include "Descriptors.hpp"
#include "Image.hpp"

class UI {
public:
    UI(Device &device, VkRenderPass renderPass, std::string dynamicShaderPath);
    ~UI();
    
    struct PushConstBlock {
		glm::vec2 scale;
		glm::vec2 translate;
	} pushConstBlock;
    
    void newFrame(Application *app);
    void updateBuffers(int frameIndex);
    void draw(VkCommandBuffer commandBuffer, int frameIndex);

private:
    std::vector<char> readFile(const std::string &filepath);
    void loadFontTexture();
    void createDescriptors();
    void createPipeline(VkRenderPass renderPass, std::string dynamicShaderPath);

    Device &device;
    Image vulkanImage{device};
    
    VkPipeline imguiPipeline;
    VkPipelineLayout imguiPipelineLayout;
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    
    std::vector<std::unique_ptr<Buffer>> *vertexBuffers;
    std::vector<std::unique_ptr<Buffer>> *indexBuffers;
    int vertexCount[SwapChain::MAX_FRAMES_IN_FLIGHT], indexCount[SwapChain::MAX_FRAMES_IN_FLIGHT];
    
    VkImage fontImage;
    VkDeviceMemory fontMem;
    VkImageView fontView;
    VkSampler fontSampler;
    
    VkDescriptorImageInfo fontDescriptorInfo;
    std::unique_ptr<DescriptorPool> imguiPool;
    std::unique_ptr<DescriptorSetLayout> imguiSetLayout;
    std::vector<VkDescriptorSet> *imguiDescriptorSets;
};

#endif /* UI_hpp */
