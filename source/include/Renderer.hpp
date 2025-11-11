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
#include "Primitive.hpp"

//std
#include <memory>
#include <vector>
#include <cassert>

enum RenderPass : unsigned int{
    WorldSpace = 0,
    ScreenSpace,
    DepthPass,
    
    TotalCount
};

class Renderer {
private:
    struct MultiFrameBufferAttachment {
        VkImage image[SwapChain::MAX_FRAMES_IN_FLIGHT];
        VkDeviceMemory mem[SwapChain::MAX_FRAMES_IN_FLIGHT];
        VkImageView view[SwapChain::MAX_FRAMES_IN_FLIGHT];
    };
    
    struct OffscreenPassAttachments {
        VkExtent2D extent{1024, 1024};
		VkFramebuffer frameBuffer[SwapChain::MAX_FRAMES_IN_FLIGHT];
		MultiFrameBufferAttachment color, depth, multisampling;
		VkRenderPass renderPass;
		VkSampler sampler[SwapChain::MAX_FRAMES_IN_FLIGHT];
		VkDescriptorImageInfo descriptorImage[SwapChain::MAX_FRAMES_IN_FLIGHT];
        static constexpr VkFormat colorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        VkFormat depthFormat;
        DescriptorStruct descriptor;
	} offscreen[RenderPass::TotalCount];

public:
    
    Renderer(SDLWindow &pasWindow, Device &passDevice);
    ~Renderer();
    
    // Prevent Obj copy
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    
    VkRenderPass getOffscreenRenderPass(RenderPass index) const { return offscreen[index].renderPass; }
    VkRenderPass getSwapChainRenderPass() const { return swapChain->getCompositionRenderPass(); }
    float getAspectRatio() const { return swapChain->extentAspectRatio(); }
    bool isFrameInProgress() const { return  isFrameStarted; }
    VkDescriptorImageInfo* getOffscreenImageDescriptor(void);
    
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
    void recreateSwapChain(bool forced = false);
    
    VkCommandBuffer beginFrame();
    void endFrame();
    void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
    void beginOffscreenRenderPass(VkCommandBuffer commandBuffer, RenderPass index);
    void endRenderPass(VkCommandBuffer commandBuffer);
    

    const VkDescriptorSetLayout* getDescriptorSetLayout(RenderPass index) { return offscreen[index].descriptor.layout->getDescriptorSetLayout(); }
    std::vector<VkDescriptorSet>* getDescriptorSets(RenderPass index) { return &offscreen[index].descriptor.v_set; }
    
    VkDescriptorImageInfo* getImageDescriptor(RenderPass index) { return offscreen[index].descriptorImage; }
    
private:
    
    void createCommandBuffers(void);
    void freeCommandBuffers(void);
    
    void createRenderPasses(void);
    void destroyRenderPasses(void);
    
    void createOffscreenPass(RenderPass index);
    void destroyOffscreenPass(RenderPass index);
    
    void createDepthPass(RenderPass index);
    void destroyDepthPass(RenderPass index);
    
    void destroyBrdfLut(void);
    bool wasBrdfRequested = false;
    
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
    
    uint32_t currentImageIndex;
    int currentFrameIndex{0};
    bool isFrameStarted = false;
};

#endif /* Renderer_hpp */
