//
//  SwapChain.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 07/11/21.
//

#ifndef SwapChain_hpp
#define SwapChain_hpp

#include "Device.hpp"

#include <string>
#include <vector>
#include <memory>

class SwapChain {
public:
    static bool VSync;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 3;
    
    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;
    
    SwapChain(const Device& deviceRef, VkExtent2D windowExtent);
    SwapChain(const Device& deviceRef, VkExtent2D windowExtent, std::shared_ptr<SwapChain> previous);
    ~SwapChain();
    
    VkRenderPass getCompositionRenderPass(void) const { return m_CompositionRenderPass; }
    VkFramebuffer getSwapChainFrameBuffer(int index) const { return m_SwapChainFramebuffers[index]; }
    VkImageView getImageView(int index) const { return m_SwapChainImageViews[index]; }
    VkFence* getCurrentImageFence(int imageIndex) { return &m_ImagesInFlight[imageIndex]; }
    VkImage getImage(int index) const { return m_SwapChainImages[index]; }
    size_t imageCount(void) const { return m_SwapChainImages.size(); }
    VkFormat getSwapChainImageFormat(void) const { return m_SwapChainImageFormat; }
    VkExtent2D getSwapChainExtent(void) const { return m_SwapChainExtent; }
    uint32_t width(void) const { return m_SwapChainExtent.width; }
    uint32_t height(void) const { return m_SwapChainExtent.height; }
    float extentAspectRatio(void) const { return static_cast<float>(m_SwapChainExtent.width) / static_cast<float>(m_SwapChainExtent.height); }
    VkFormat findDepthFormat(void) const;
    
    VkResult acquireNextImage(uint32_t *imageIndex);
    VkResult submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex);
    
private:
    void init(void);
    void createSwapChain(void);
    void createImageViews(void);
    void createDepthStencilResources(void);
    void createCompositionRenderPass(void);
    void createSwapChainFramebuffers(void);
    void createSyncObjects(void);
    
    // Helper functions
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    
    struct FrameBufferAttachment {
        VkImage image;
        VkDeviceMemory mem;
        VkImageView view;
    };
    
    const Device& m_Device;
    VkSwapchainKHR m_SwapChainHandle;
    std::shared_ptr<SwapChain> m_OldSwapChain;
    
    VkExtent2D m_WindowExtent;
    VkExtent2D m_SwapChainExtent;
    VkFormat m_SwapChainImageFormat;
    std::vector<VkImage> m_SwapChainImages;
    std::vector<VkImageView> m_SwapChainImageViews;
    std::vector<VkFramebuffer> m_SwapChainFramebuffers;
    VkRenderPass m_CompositionRenderPass;
    
    FrameBufferAttachment m_DepthStencil;
    
    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;
    std::vector<VkFence> m_ImagesInFlight;
    
    size_t m_CurrentFrame = 0;
};

#endif /* SwapChain_hpp */
