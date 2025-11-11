//
//  Renderer.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 05/11/21.
//

#include "Renderer.hpp"

#include <array>
#include <cassert>
#include <stdexcept>
#include <iostream>

Renderer::Renderer(SDLWindow &passWindow, Device &passDevice) : window{passWindow}, device{passDevice} {
    recreateSwapChain();
    
    createCommandBuffers();
}

Renderer::~Renderer() {
    freeCommandBuffers();
    
    destroyRenderPasses();
    
    destroyBrdfLut();
}

RenderPass& operator++(RenderPass& orig)
{
  orig = (orig < RenderPass::TotalCount) ? static_cast<RenderPass>(orig + 1) : RenderPass::TotalCount;
  return orig;
}

void Renderer::createRenderPasses(void) {
    for (RenderPass p{}; p < RenderPass::TotalCount; ++p) {
        switch (p) {
            case RenderPass::DepthPass:
                createDepthPass(p);
                break;
                
            default:
                createOffscreenPass(p);
                break;
        }
    }
}

void Renderer::destroyRenderPasses(void) {
    for (RenderPass p{}; p < RenderPass::TotalCount; ++p) {
        switch (p) {
            case RenderPass::DepthPass:
                destroyDepthPass(p);
                break;
                
            default:
                destroyOffscreenPass(p);
                break;
        }
    }
}

void Renderer::recreateSwapChain(bool forced) {
    VkExtent2D actualExtent = window.getSurfaceExtent();
    while (actualExtent.width == 0 || actualExtent.height == 0) {
        actualExtent = window.getSurfaceExtent();
    }
    vkDeviceWaitIdle(device.device());
    
    if (swapChain == nullptr) {
        swapChain = std::make_unique<SwapChain>(device, actualExtent);
        createRenderPasses();
    } else {
        VkExtent2D oldExtent = swapChain->getSwapChainExtent();
        swapChain = std::make_unique<SwapChain>(device, actualExtent, std::move(swapChain));
        if(oldExtent.width != actualExtent.width || oldExtent.height != actualExtent.height || forced) {
            destroyRenderPasses();
            createRenderPasses();
        }
    }
}

void Renderer::createCommandBuffers() {
    commandBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = device.getCommandPool();
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
    
    if (vkAllocateCommandBuffers(device.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }
}

void Renderer::freeCommandBuffers() {
    vkFreeCommandBuffers(
        device.device(),
        device.getCommandPool(),
        static_cast<uint32_t>(commandBuffers.size()),
        commandBuffers.data()
    );
    commandBuffers.clear();
}

VkCommandBuffer Renderer::beginFrame() {
    assert(!isFrameStarted && "Can't call beginFrame while already in progress");
    
    auto result = swapChain->acquireNextImage(&currentImageIndex);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain(true);
        return nullptr;
    }
    
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image");
    }
    
    isFrameStarted = true;
    
    auto commandBuffer = getCurrentCommandBuffer();
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffers");
    }
    
    return commandBuffer;
}

void Renderer::endFrame() {
    assert(isFrameStarted && "Can't call endFrame while frame is not in progress");
    auto commandBuffer = getCurrentCommandBuffer();
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
    }
    
    auto result = swapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapChain(true);
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to acquire swap chain image");
    }
    
    isFrameStarted = false;
    currentFrameIndex = (currentFrameIndex + 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;
}

void Renderer::beginOffscreenRenderPass(VkCommandBuffer commandBuffer, RenderPass index) {
    OffscreenPassAttachments& attachments = offscreen[index];
    
    assert(isFrameStarted && "Can't call endFrame while frame is not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() &&
        "Can't begin render pass on command buffer from a different frame");
    
    VkRenderPassBeginInfo renderpassInfo{};
    renderpassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderpassInfo.renderPass = attachments.renderPass;
    renderpassInfo.framebuffer = attachments.frameBuffer[currentImageIndex];
    
    renderpassInfo.renderArea.offset = {0, 0};
    renderpassInfo.renderArea.extent = attachments.extent;
    
    std::array<VkClearValue, 2> clearValues{};
    if(index != RenderPass::DepthPass) {
        clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f};
        clearValues[1].depthStencil = {1.0f, 0};
        renderpassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderpassInfo.pClearValues = clearValues.data();
    } else {
        clearValues[0].depthStencil = {1.0f, 0};
        renderpassInfo.clearValueCount = 1;
        renderpassInfo.pClearValues = clearValues.data();
    }
    
    vkCmdBeginRenderPass(commandBuffer, &renderpassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(attachments.extent.width);
    viewport.height = static_cast<float>(attachments.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, attachments.extent};
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void Renderer::beginSwapChainRenderPass(VkCommandBuffer commandBuffer) {
    assert(isFrameStarted && "Can't call endFrame while frame is not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() &&
        "Can't begin render pass on command buffer from a different frame");
    
    VkRenderPassBeginInfo renderpassInfo{};
    renderpassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderpassInfo.renderPass = swapChain->getCompositionRenderPass();
    renderpassInfo.framebuffer = swapChain->getSwapChainFrameBuffer(currentImageIndex);
    
    renderpassInfo.renderArea.offset = {0, 0};
    renderpassInfo.renderArea.extent = swapChain->getSwapChainExtent();
    
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    renderpassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderpassInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(commandBuffer, &renderpassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChain->getSwapChainExtent().width);
    viewport.height = static_cast<float>(swapChain->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, swapChain->getSwapChainExtent()};
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void Renderer::endRenderPass(VkCommandBuffer commandBuffer) {
    assert(isFrameStarted && "Can't call endFrame while frame is not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() &&
        "Can't end render pass on command buffer from a different frame");
    vkCmdEndRenderPass(commandBuffer);
}
