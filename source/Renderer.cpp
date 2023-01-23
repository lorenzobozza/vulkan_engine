//
//  Renderer.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 05/11/21.
//

#include "include/Renderer.hpp"

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
    
    destroyOffscreenPass();
    
    vkDestroySampler(device.device(), brdfSampler, nullptr);
    vkDestroyImageView(device.device(), brdf.view, nullptr);
    vkDestroyImage(device.device(), brdf.image, nullptr);
    vkFreeMemory(device.device(), brdf.mem, nullptr);
}

void Renderer::recreateSwapChain() {
    auto extent = window.getExtent();
    while (extent.width == 0 || extent.height == 0) {
        extent = window.getExtent();
        //glfwWaitEvents();
    }
    vkDeviceWaitIdle(device.device());
    
    if (swapChain == nullptr) {
        swapChain = std::make_unique<SwapChain>(device, extent);
        createOffscreenPass();
    } else {
        std::shared_ptr<SwapChain> oldSwapChain = std::move(swapChain);
        swapChain = std::make_unique<SwapChain>(device, extent, std::move(swapChain));
        if(oldSwapChain->getSwapChainExtent().width != extent.width || oldSwapChain->getSwapChainExtent().height != extent.height || recreateOffscreenFlag) {
            destroyOffscreenPass();
            createOffscreenPass();
            recreateOffscreenFlag = false;
        }
        //if (!oldSwapChain->compareSwapFormats(*swapChain.get())) {
        //    throw std::runtime_error("Swap chain image(or depth) format has changed");
        //}
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
        recreateSwapChain();
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
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR/* || window.wasWindowResized()*/) {
        //window.resetWindowResizeFlag();
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to acquire swap chain image");
    }
    
    isFrameStarted = false;
    currentFrameIndex = (currentFrameIndex + 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;
}

void Renderer::beginOffscreenRenderPass(VkCommandBuffer commandBuffer) {
    assert(isFrameStarted && "Can't call endFrame while frame is not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() &&
        "Can't begin render pass on command buffer from a different frame");
    
    VkRenderPassBeginInfo renderpassInfo{};
    renderpassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderpassInfo.renderPass = offscreen.renderPass;
    renderpassInfo.framebuffer = offscreen.frameBuffer;
    
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

void Renderer::endOffscreenRenderPass(VkCommandBuffer commandBuffer) {
    assert(isFrameStarted && "Can't call endFrame while frame is not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() &&
        "Can't end render pass on command buffer from a different frame");
    vkCmdEndRenderPass(commandBuffer);
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

void Renderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer) {
    assert(isFrameStarted && "Can't call endFrame while frame is not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() &&
        "Can't end render pass on command buffer from a different frame");
    vkCmdEndRenderPass(commandBuffer);
}

void Renderer::createOffscreenPass() {
    // Color Resources
    VkExtent2D swapChainExtent = getSwapChainExtent();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = swapChainExtent.width;
    imageInfo.extent.height = swapChainExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = offscreen.colorFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage =  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = 0;

    device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, offscreen.color.image, offscreen.color.mem);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = offscreen.color.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = offscreen.colorFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device.device(), &viewInfo, nullptr, &offscreen.color.view) != VK_SUCCESS) {
      throw std::runtime_error("failed to create texture image view!");
    }
    
    // Depth Resources
    offscreen.depthFormat = swapChain->findDepthFormat();

    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = swapChainExtent.width;
    imageInfo.extent.height = swapChainExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = offscreen.depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = device.msaaSamples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = 0;

    device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, offscreen.depth.image, offscreen.depth.mem);

    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = offscreen.depth.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = offscreen.depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device.device(), &viewInfo, nullptr, &offscreen.depth.view) != VK_SUCCESS) {
      throw std::runtime_error("failed to create texture image view!");
    }
    
    // Multisampling Resources
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = swapChainExtent.width;
    imageInfo.extent.height = swapChainExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = offscreen.colorFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage =  VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.samples = device.msaaSamples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = 0;

    device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, offscreen.multisampling.image, offscreen.multisampling.mem);

    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = offscreen.multisampling.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = offscreen.colorFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device.device(), &viewInfo, nullptr, &offscreen.multisampling.view) != VK_SUCCESS) {
      throw std::runtime_error("failed to create texture image view!");
    }
    
    // Renderpass
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = offscreen.colorFormat;
    colorAttachment.samples = device.msaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = device.msaaSamples == VK_SAMPLE_COUNT_1_BIT ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = offscreen.depthFormat;
    depthAttachment.samples = device.msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format = offscreen.colorFormat;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    if (device.msaaSamples != VK_SAMPLE_COUNT_1_BIT) { subpass.pResolveAttachments = &colorAttachmentResolveRef; }

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.srcAccessMask = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstSubpass = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      
    std::vector<VkAttachmentDescription> attachments;
    if (device.msaaSamples == VK_SAMPLE_COUNT_1_BIT) {
        attachments = {colorAttachment, depthAttachment};
    } else {
        attachments = {colorAttachment, depthAttachment, colorAttachmentResolve};
    }

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device.device(), &renderPassInfo, nullptr, &offscreen.renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
    
    // Framebuffer
    std::vector<VkImageView> imageViewAttachments;
    if (device.msaaSamples == VK_SAMPLE_COUNT_1_BIT) {
        imageViewAttachments = {offscreen.color.view, offscreen.depth.view};
    } else {
        imageViewAttachments = {offscreen.multisampling.view, offscreen.depth.view, offscreen.color.view};
    }

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = offscreen.renderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = imageViewAttachments.data();
    framebufferInfo.width = swapChainExtent.width;
    framebufferInfo.height = swapChainExtent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(device.device(), &framebufferInfo, nullptr, &offscreen.frameBuffer) != VK_SUCCESS) {
      throw std::runtime_error("failed to create framebuffer!");
    }
    
    // Image Sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = samplerInfo.addressModeU;
    samplerInfo.addressModeW = samplerInfo.addressModeU;
    
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = device.properties.limits.maxSamplerAnisotropy;
    
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    
    if (vkCreateSampler(device.device(), &samplerInfo, nullptr, &offscreen.sampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create offscreen sampler!");
    }
    
    VkDescriptorImageInfo offscreenDescriptorInfo{
        offscreen.sampler,
        offscreen.color.view,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    
    // Post-processing Pipeline
    postprocPool =
       DescriptorPool::Builder(device)
           .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .build();
    
    postprocSetLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
    
    postprocDescriptorSets = new std::vector<VkDescriptorSet>(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < postprocDescriptorSets->size(); i++) {
        DescriptorWriter(*postprocSetLayout, *postprocPool)
            .writeImage(0, &offscreenDescriptorInfo)
            .build(postprocDescriptorSets->at(i));
    }
}

void Renderer::destroyOffscreenPass() {
    vkDestroySampler(device.device(), offscreen.sampler, nullptr);
    vkDestroyFramebuffer(device.device(), offscreen.frameBuffer, nullptr);
    vkDestroyRenderPass(device.device(), offscreen.renderPass, nullptr);
    
    vkDestroyImageView(device.device(), offscreen.color.view, nullptr);
    vkDestroyImage(device.device(), offscreen.color.image, nullptr);
    vkFreeMemory(device.device(), offscreen.color.mem, nullptr);

    vkDestroyImageView(device.device(), offscreen.depth.view, nullptr);
    vkDestroyImage(device.device(), offscreen.depth.image, nullptr);
    vkFreeMemory(device.device(), offscreen.depth.mem, nullptr);
    
    vkDestroyImageView(device.device(), offscreen.multisampling.view, nullptr);
    vkDestroyImage(device.device(), offscreen.multisampling.image, nullptr);
    vkFreeMemory(device.device(), offscreen.multisampling.mem, nullptr);
    
    // TODO: migrate all deletions to unique_ptr = nullptr
    delete(postprocDescriptorSets);
    postprocSetLayout.reset();
    postprocPool.reset();
}

void Renderer::integrateBrdfLut(std::string shaderPath) {
    VkAttachmentDescription attachment = {};

    attachment.format = VK_FORMAT_R16G16_SFLOAT;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorReference = {};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;
    
    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassCreateInfo{};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &attachment;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount = 2;
    renderPassCreateInfo.pDependencies = dependencies.data();
    
    VkRenderPass renderPass;

    if (vkCreateRenderPass(device.device(), &renderPassCreateInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create brdf renderpass!");
    }

    VkPipelineLayout pipelineLayout;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;
    if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }
    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;
    pipelineConfig.colorBlendAttachment.blendEnable = VK_FALSE;
    pipelineConfig.rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    Pipeline pipeline{
        device,
        shaderPath+"brdf.vert.spv",
        shaderPath+"brdf.frag.spv",
        pipelineConfig
    };
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = 512;
    imageInfo.extent.height = 512;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R16G16_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage =  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = 0;

    device.createImageWithInfo(
        imageInfo,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        brdf.image,
        brdf.mem);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = brdf.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R16G16_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device.device(), &viewInfo, nullptr, &brdf.view) != VK_SUCCESS) {
      throw std::runtime_error("failed to create brdf image view!");
    }
        
    VkFramebuffer frameBuffer;
        
    VkFramebufferCreateInfo fbufCreateInfo{};
    fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbufCreateInfo.renderPass = renderPass;
    fbufCreateInfo.attachmentCount = 1;
    fbufCreateInfo.pAttachments = &brdf.view;
    fbufCreateInfo.width = 512;
    fbufCreateInfo.height = 512;
    fbufCreateInfo.layers = 1;

	if (vkCreateFramebuffer(device.device(), &fbufCreateInfo, nullptr, &frameBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create brdf framebuffer!");
    }
    
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = samplerInfo.addressModeU;
    samplerInfo.addressModeW = samplerInfo.addressModeU;
    
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = device.properties.limits.maxSamplerAnisotropy;
    
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    
    if (vkCreateSampler(device.device(), &samplerInfo, nullptr, &brdfSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create brdf sampler!");
    }
    
    VkCommandBuffer commandBuffer;
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = device.getCommandPool();
    allocInfo.commandBufferCount = 1;
    
    if (vkAllocateCommandBuffers(device.device(), &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffers");
    }
    
    VkRenderPassBeginInfo renderpassInfo{};
    renderpassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderpassInfo.renderPass = renderPass;
    renderpassInfo.framebuffer = frameBuffer;
    
    renderpassInfo.renderArea.offset = {0, 0};
    renderpassInfo.renderArea.extent.width = 512;
    renderpassInfo.renderArea.extent.height = 512;
    
    VkClearValue clearValue{};
    clearValue.color = {0.f, 0.f, 0.f, 0.f};
    renderpassInfo.clearValueCount = 1;
    renderpassInfo.pClearValues = &clearValue;
    
    vkCmdBeginRenderPass(commandBuffer, &renderpassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = 512;
    viewport.height = 512;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, {512, 512}};
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    //RENDER
    Model::Data data;
    data.vertices = {
        {{-1.f, -1.f, .0f}, {}, {}, {}, {0.f, 0.f}},
        {{1.f, -1.f, .0f}, {}, {}, {}, {1.f, 0.f}},
        {{1.f, 1.f, .0f}, {}, {}, {}, {1.f, 1.f}},
        {{-1.f, 1.f, .0f}, {}, {}, {}, {0.f, 1.f}}
    };
    data.indices = {
        0,1,2,3,0,2
    };
    
    Model quad{device, data};
    
    pipeline.bind(commandBuffer);
    
    quad.bind(commandBuffer);
    quad.draw(commandBuffer);
    
    vkCmdEndRenderPass(commandBuffer);
    
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
    }
    
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    if (vkQueueSubmit(device.graphicsQueue(), 1, &submitInfo, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }
    
    vkDeviceWaitIdle(device.device());
    
    vkFreeCommandBuffers(
        device.device(),
        device.getCommandPool(),
        1,
        &commandBuffer
    );
    commandBuffer = VK_NULL_HANDLE;
    
    vkDestroyFramebuffer(device.device(), frameBuffer, nullptr);
    vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
    vkDestroyRenderPass(device.device(), renderPass, nullptr);
    
    brdfImageInfo = VkDescriptorImageInfo {
        brdfSampler,
        brdf.view,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
}
