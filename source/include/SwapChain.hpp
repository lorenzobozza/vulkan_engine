//
//  SwapChain.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 07/11/21.
//

#ifndef SwapChain_hpp
#define SwapChain_hpp

#include "Device.hpp"

// vulkan headers
#include <vulkan/vulkan.h>

// std lib headers
#include <memory>
#include <string>
#include <vector>

class SwapChain {
 public:
  static constexpr int MAX_FRAMES_IN_FLIGHT = 3;
  
  static bool enableVSync;

  SwapChain(Device &deviceRef, VkExtent2D windowExtent);
  SwapChain(Device &deviceRef, VkExtent2D windowExtent, std::shared_ptr<SwapChain> previous);
  ~SwapChain();

  SwapChain(const SwapChain &) = delete;
  SwapChain &operator=(const SwapChain &) = delete;

  VkRenderPass getCompositionRenderPass() { return compositionRenderPass; }
  VkFramebuffer getSwapChainFrameBuffer(int index) { return swapChainFramebuffers[index]; }
  VkImage getImage(int index) { return swapChainImages[index]; }
  VkImageView getImageView(int index) { return swapChainImageViews[index]; }
  VkFence *getCurrentImageFence(int imageIndex) { return &imagesInFlight[imageIndex]; }
  size_t imageCount() { return swapChainImages.size(); }
  VkFormat getSwapChainImageFormat() { return swapChainImageFormat; }
  VkExtent2D getSwapChainExtent() { return swapChainExtent; }
  bool isVSyncEnabled() { return enableVSync; }
  uint32_t width() { return swapChainExtent.width; }
  uint32_t height() { return swapChainExtent.height; }

  float extentAspectRatio() {
    return static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height);
  }
  VkFormat findDepthFormat();

  VkResult acquireNextImage(uint32_t *imageIndex);
  VkResult submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex);

/*
    bool compareSwapFormats(const SwapChain &swap_chain) const {
        return swap_chain.offscreenDepthFormat == offscreenDepthFormat &&
            swap_chain.offscreenImageFormat == offscreenImageFormat;
    } //MAYBE relative to final swapchain format
*/

 private:
    void init();
    void createSwapChain();
    void createImageViews();
    void createDepthStencilResources();
    void createCompositionRenderPass();
    void createSwapChainFramebuffers();
    void createSyncObjects();
    
    // Helper functions
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR> &availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(
      const std::vector<VkPresentModeKHR> &availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);
    
    struct FrameBufferAttachment {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
	};
    FrameBufferAttachment depthStencil;
 
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
 
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkRenderPass compositionRenderPass;
    
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;

    Device &device;
    VkExtent2D windowExtent;

    VkSwapchainKHR swapChain;
    std::shared_ptr<SwapChain> oldSwapChain;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    size_t currentFrame = 0;
};

#endif /* SwapChain_hpp */
