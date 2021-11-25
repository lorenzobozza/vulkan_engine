//
//  Renderer.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 05/11/21.
//

#ifndef Renderer_hpp
#define Renderer_hpp

#include "Window.hpp"
#include "Device.hpp"
#include "SwapChain.hpp"

//std
#include <memory>
#include <vector>
#include <cassert>

class Renderer {
public:
    
    Renderer(Window &pasWindow, Device &passDevice);
    ~Renderer();
    
    // Prevent Obj copy
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    
    VkRenderPass getSwapChainRenderPass() const { return swapChain->getRenderPass(); }
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
    
    VkCommandBuffer beginFrame();
    void endFrame();
    void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
    void endSwapChainRenderPass(VkCommandBuffer commandBuffer);
    
private:
    void recreateSwapChain();
    void createCommandBuffers();
    void freeCommandBuffers();

    Window &window;
    Device &device;
    std::unique_ptr<SwapChain> swapChain;
    std::vector<VkCommandBuffer> commandBuffers;
    
    uint32_t currentImageIndex;
    int currentFrameIndex{0};
    bool isFrameStarted = false;
};

#endif /* Renderer_hpp */
