//
//  Renderer.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 05/11/21.
//

#ifndef Renderer_hpp
#define Renderer_hpp

#include "SDLWindow.hpp"
#include "Device.hpp"
#include "SwapChain.hpp"
#include "Descriptors.hpp"
#include "Pipeline.hpp"
#include "SolidObject.hpp"

//std
#include <memory>
#include <vector>
#include <cassert>

class Renderer {
public:
    
    Renderer(SDLWindow &pasWindow, Device &passDevice);
    ~Renderer();
    
    // Prevent Obj copy
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    
    VkRenderPass getOffscreenRenderPass() const { return offscreen.renderPass; }
    VkRenderPass getSwapChainRenderPass() const { return swapChain->getCompositionRenderPass(); }
    float getAspectRatio() const { return swapChain->extentAspectRatio(); }
    bool isFrameInProgress() const { return  isFrameStarted; }
    
    VkCommandBuffer getCurrentCommandBuffer() const {
        assert(isFrameStarted && "Cannot get command buffer when frame not in progress");
        return commandBuffers[currentFrameIndex];
    }
    
    int getFrameIndex() const {
        assert(isFrameStarted && "Cannot get frame index when frame not in progress");
        return currentImageIndex;
    }
    
    VkImage getImage(int index) { return swapChain->getImage(index); }
    VkExtent2D getSwapChainExtent() { return swapChain->getSwapChainExtent(); }
    VkFence *getSwapChainImageFence(int imageIndex) { return swapChain->getCurrentImageFence(imageIndex); }
    
    void integrateBrdfLut(std::string shaderPath);
    VkDescriptorImageInfo* getBrdfLutInfo() { return &brdfImageInfo; }

    bool isVSyncEnabled() { return swapChain->isVSyncEnabled(); }
    void recreateSwapChain();
    bool recreateOffscreenFlag = false;
    
    VkCommandBuffer beginFrame();
    void endFrame();
    void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
    void endSwapChainRenderPass(VkCommandBuffer commandBuffer);
    void beginOffscreenRenderPass(VkCommandBuffer commandBuffer);
    void endOffscreenRenderPass(VkCommandBuffer commandBuffer);
    
    VkDescriptorSetLayout getPostProcessingDescriptorSetLayout() { return postprocSetLayout->getDescriptorSetLayout(); }
    std::vector<VkDescriptorSet> *getPostProcessingDescriptorSets() { return postprocDescriptorSets; }
    
private:
    void createCommandBuffers();
    void freeCommandBuffers();
    
    void createOffscreenPass();
    void destroyOffscreenPass();
    
    struct FrameBufferAttachment {
        VkImage image;
        VkDeviceMemory mem;
        VkImageView view;
    };
    
    SDLWindow &window;
    Device &device;
    std::unique_ptr<SwapChain> swapChain;
    std::vector<VkCommandBuffer> commandBuffers;
    
    FrameBufferAttachment brdf;
    VkSampler brdfSampler;
    VkDescriptorImageInfo brdfImageInfo;
    
    struct OffscreenPass {
		int32_t width, height;
		VkFramebuffer frameBuffer;
		FrameBufferAttachment color, depth, multisampling;
		VkRenderPass renderPass;
		VkSampler sampler;
		VkDescriptorImageInfo descriptor;
        const VkFormat colorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        VkFormat depthFormat;
	} offscreen;
    
    std::unique_ptr<DescriptorPool> postprocPool;
    std::unique_ptr<DescriptorSetLayout> postprocSetLayout;
    std::vector<VkDescriptorSet> *postprocDescriptorSets;
    
    uint32_t currentImageIndex;
    int currentFrameIndex{0};
    bool isFrameStarted = false;
};

#endif /* Renderer_hpp */
