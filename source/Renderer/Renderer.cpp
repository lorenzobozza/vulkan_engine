//
//  Renderer.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 05/11/21.
//

#include <array>
#include <stdexcept>

#include "Renderer.hpp"
#include "Log.hpp"

Renderer::Renderer(SDLWindow &passWindow, const Device& passDevice, VkSampleCountFlagBits& msaaSampleCount)
: m_Window(passWindow), m_Device(passDevice), m_MSAASampleCount(msaaSampleCount) {
    recreateSwapChain();
    createCommandBuffers();
}

Renderer::~Renderer() {
    freeCommandBuffers();
    destroyRenderPasses(true);
    destroyBrdfLut();
}

RenderPass& operator++(RenderPass& orig)
{
    orig = (orig < RenderPass::TotalCount) ? static_cast<RenderPass>(orig + 1) : RenderPass::TotalCount;
    return orig;
}

void Renderer::createRenderPasses(bool all) {
    for (RenderPass p{}; p < RenderPass::TotalCount; ++p) {
        switch (p) {
        case RenderPass::DepthPass:
            createDepthPass(p);
            break;
            
        case RenderPass::ShadowPass:
            if (all) createDepthPass(p);
            break;
            
        default:
            createOffscreenPass(p);
            break;
        }
    }
}

void Renderer::destroyRenderPasses(bool all) {
    for (RenderPass p{}; p < RenderPass::TotalCount; ++p) {
        switch (p) {
        case RenderPass::DepthPass:
            destroyDepthPass(p);
            break;
            
        case RenderPass::ShadowPass:
            if (all) destroyDepthPass(p);
            break;
            
        default:
            destroyOffscreenPass(p);
            break;
        }
    }
}

void Renderer::recreateSwapChain(bool forced) {
    VkExtent2D actualExtent = m_Window.getSurfaceExtent();
    while (actualExtent.width == 0 || actualExtent.height == 0) {
        actualExtent = m_Window.getSurfaceExtent();
    }
    vkDeviceWaitIdle(m_Device.device());
    
    if (m_SwapChain == nullptr) {
        m_SwapChain = std::make_unique<SwapChain>(m_Device, actualExtent);
        createRenderPasses(true);
    } else {
        VkExtent2D oldExtent = m_SwapChain->getSwapChainExtent();
        m_SwapChain = std::make_unique<SwapChain>(m_Device, actualExtent, std::move(m_SwapChain));
        if(oldExtent.width != actualExtent.width || oldExtent.height != actualExtent.height || forced) {
            destroyRenderPasses();
            createRenderPasses();
        }
    }
}

void Renderer::createCommandBuffers(void) {
    m_CommandBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_Device.getCommandPool();
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_CommandBuffers.size());
    
    if (vkAllocateCommandBuffers(m_Device.device(), &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }
}

void Renderer::freeCommandBuffers(void) {
    vkFreeCommandBuffers(m_Device.device(), m_Device.getCommandPool(),
                         static_cast<uint32_t>(m_CommandBuffers.size()),
                         m_CommandBuffers.data());
    m_CommandBuffers.clear();
}

VkCommandBuffer Renderer::beginFrame(void) {
    assert(!m_IsFrameStarted && "Can't call beginFrame while already in progress");
    
    auto result = m_SwapChain->acquireNextImage(&m_CurrentImageIndex);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapChain();
        return nullptr;
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to acquire swap chain image");
    }
    
    m_IsFrameStarted = true;
    
    auto commandBuffer = getCurrentCommandBuffer();
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffers");
    }
    
    return commandBuffer;
}

void Renderer::endFrame(void) {
    assert(m_IsFrameStarted && "Can't call endFrame while frame is not in progress");
    auto commandBuffer = getCurrentCommandBuffer();
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
    }
    
    auto result = m_SwapChain->submitCommandBuffers(&commandBuffer, &m_CurrentImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to acquire swap chain image");
    }
    
    m_IsFrameStarted = false;
    m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;
}

void Renderer::beginOffscreenRenderPass(VkCommandBuffer commandBuffer, RenderPass index) {
    OffscreenPassAttachments& attachments = m_Offscreen[index];
    
    assert(m_IsFrameStarted && "Can't call endFrame while frame is not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() &&
           "Can't begin render pass on command buffer from a different frame");
    
    VkRenderPassBeginInfo renderpassInfo{};
    renderpassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderpassInfo.renderPass = attachments.renderPass;
    renderpassInfo.framebuffer = attachments.frameBuffer[m_CurrentImageIndex];
    
    renderpassInfo.renderArea.offset = {0, 0};
    renderpassInfo.renderArea.extent = attachments.extent;
    
    std::array<VkClearValue, 2> clearValues{};
    if(index != RenderPass::DepthPass && index != RenderPass::ShadowPass) {
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
    assert(m_IsFrameStarted && "Can't call endFrame while frame is not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() &&
           "Can't begin render pass on command buffer from a different frame");
    
    VkRenderPassBeginInfo renderpassInfo{};
    renderpassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderpassInfo.renderPass = m_SwapChain->getCompositionRenderPass();
    renderpassInfo.framebuffer = m_SwapChain->getSwapChainFrameBuffer(m_CurrentImageIndex);
    
    renderpassInfo.renderArea.offset = {0, 0};
    renderpassInfo.renderArea.extent = m_SwapChain->getSwapChainExtent();
    
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    renderpassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderpassInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(commandBuffer, &renderpassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_SwapChain->getSwapChainExtent().width);
    viewport.height = static_cast<float>(m_SwapChain->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, m_SwapChain->getSwapChainExtent()};
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void Renderer::endRenderPass(VkCommandBuffer commandBuffer) {
    assert(m_IsFrameStarted && "Can't call endFrame while frame is not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() &&
           "Can't end render pass on command buffer from a different frame");
    vkCmdEndRenderPass(commandBuffer);
}

VkCommandBuffer Renderer::getCurrentCommandBuffer() const {
    assert(m_IsFrameStarted && "Cannot get command buffer when frame not in progress");
    return m_CommandBuffers[m_CurrentFrameIndex];
}

int Renderer::getFrameIndex() const {
    assert(m_IsFrameStarted && "Cannot get frame index when frame not in progress");
    return m_CurrentImageIndex;
}

void Renderer::graphic2GraphicMemoryBarrier(VkCommandBuffer commandBuffer, RenderPass pass, int frameIndex) {
    static VkImageMemoryBarrier s_Barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        // Synchronization
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        // Layout transition
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        // No queue ownership transfer
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .subresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };
    
    s_Barrier.image = m_Offscreen[pass].color.image[frameIndex];
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // Source pipeline stage
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,          // Destination pipeline stage
        0,              // dependency flags
        0, NULL,        // memory barriers
        0, NULL,        // buffer barriers
        1, &s_Barrier   // image barriers
    );
}
