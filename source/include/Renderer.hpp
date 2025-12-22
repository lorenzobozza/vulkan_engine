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

#include <vector>
#include <cassert>

enum RenderPass : unsigned int{
    WorldSpace = 0,
    ScreenSpace,
    ShadowPass,
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
    } m_Offscreen[RenderPass::TotalCount];
    
public:
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;
    
    Renderer(SDLWindow& pasWindow, const Device& passDevice, VkSampleCountFlagBits& msaaSampleCount);
    ~Renderer();
    
    bool isFrameInProgress(void) const { return m_IsFrameStarted; }
    VkImage getImage(int index) const { return m_SwapChain->getImage(index); }
    float getAspectRatio(void) const { return m_SwapChain->extentAspectRatio(); }
    VkRenderPass getSwapChainRenderPass(void) const { return m_SwapChain->getCompositionRenderPass(); }
    VkRenderPass getOffscreenRenderPass(RenderPass index) const { return m_Offscreen[index].renderPass; }
    VkExtent2D getSwapChainExtent(void) const { return m_SwapChain->getSwapChainExtent(); }
    VkFence *getSwapChainImageFence(int imageIndex) const { return m_SwapChain->getCurrentImageFence(imageIndex); }
    VkCommandBuffer getCurrentCommandBuffer(void) const;
    int getFrameIndex(void) const;
    
    void integrateBrdfLut(void);
    VkDescriptorImageInfo* getBrdfLutInfo(void) { return &m_BrdfImageInfo; }
    
    void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
    void beginOffscreenRenderPass(VkCommandBuffer commandBuffer, RenderPass index);
    void endRenderPass(VkCommandBuffer commandBuffer);
    void recreateSwapChain(bool forced = false);
    VkCommandBuffer beginFrame(void);
    void endFrame(void);
    
    const VkDescriptorSetLayout* getDescriptorSetLayout(RenderPass index) const { return m_Offscreen[index].descriptor.layout->getDescriptorSetLayout(); }
    std::vector<VkDescriptorSet>* getDescriptorSets(RenderPass index) { return &m_Offscreen[index].descriptor.v_set; }
    VkDescriptorImageInfo* getImageDescriptor(RenderPass index) { return m_Offscreen[index].descriptorImage; }
    
private:
    void createCommandBuffers(void);
    void freeCommandBuffers(void);
    void createRenderPasses(bool all = false);
    void destroyRenderPasses(bool all = false);
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
    
    SDLWindow &m_Window;
    const Device& m_Device;
    std::unique_ptr<SwapChain> m_SwapChain;
    std::vector<VkCommandBuffer> m_CommandBuffers;
    
    VkSampleCountFlagBits& m_MSAASampleCount;
    uint32_t m_CurrentImageIndex;
    int m_CurrentFrameIndex{0};
    bool m_IsFrameStarted = false;
    
    FrameBufferAttachment m_Brdf;
    VkSampler m_BrdfSampler;
    VkDescriptorImageInfo m_BrdfImageInfo;
};

#endif /* Renderer_hpp */
