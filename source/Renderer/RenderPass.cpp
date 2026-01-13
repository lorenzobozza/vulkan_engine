//
//  RenderPass.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 25/10/25.
//

#include "Renderer.hpp"

#include <array>
#include <stdexcept>

void Renderer::createOffscreenPass(RenderPass index) {
    OffscreenPassAttachments& attachments = m_Offscreen[index];
    VkSampleCountFlagBits sampleCount = (index == RenderPass::ScreenSpace) ? VK_SAMPLE_COUNT_1_BIT : m_MSAASampleCount;
    
    // Color Resources
    attachments.extent = getSwapChainExtent();
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = attachments.extent.width;
    imageInfo.extent.height = attachments.extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = attachments.colorFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage =  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = 0;
    
    for (int f = 0; f < SwapChain::MAX_FRAMES_IN_FLIGHT; f++) {
        m_Device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, attachments.color.image[f], attachments.color.mem[f]);
        
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = attachments.color.image[f];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = attachments.colorFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(m_Device.device(), &viewInfo, nullptr, &attachments.color.view[f]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }
    }
    
    // Depth Resources
    attachments.depthFormat = m_SwapChain->findDepthFormat();
    
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = attachments.extent.width;
    imageInfo.extent.height = attachments.extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = attachments.depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = sampleCount;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = 0;
    
    for (int f = 0; f < SwapChain::MAX_FRAMES_IN_FLIGHT; f++) {
        m_Device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, attachments.depth.image[f], attachments.depth.mem[f]);
        
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = attachments.depth.image[f];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = attachments.depthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(m_Device.device(), &viewInfo, nullptr, &attachments.depth.view[f]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }
    }
    
    // Multisampling Resources
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = attachments.extent.width;
    imageInfo.extent.height = attachments.extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = attachments.colorFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage =  VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.samples = sampleCount;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = 0;
    
    for (int f = 0; f < SwapChain::MAX_FRAMES_IN_FLIGHT; f++) {
        m_Device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, attachments.multisampling.image[f], attachments.multisampling.mem[f]);
        
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = attachments.multisampling.image[f];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = attachments.colorFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(m_Device.device(), &viewInfo, nullptr, &attachments.multisampling.view[f]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }
    }
    
    // Renderpass
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = attachments.colorFormat;
    colorAttachment.samples = sampleCount;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = sampleCount == VK_SAMPLE_COUNT_1_BIT ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = attachments.depthFormat;
    depthAttachment.samples = sampleCount;
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
    colorAttachmentResolve.format = attachments.colorFormat;
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
    
    std::array<VkSubpassDescription, 1> subpasses{};
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].colorAttachmentCount = 1;
    subpasses[0].pColorAttachments = &colorAttachmentRef;
    subpasses[0].pDepthStencilAttachment = &depthAttachmentRef;
    if (sampleCount != VK_SAMPLE_COUNT_1_BIT) { subpasses[0].pResolveAttachments = &colorAttachmentResolveRef; }
    
//    VkAttachmentReference inputReference = { 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
//    subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
//    subpasses[1].colorAttachmentCount = 1;
//    subpasses[1].pColorAttachments = &colorAttachmentRef;
//    subpasses[1].pDepthStencilAttachment = &depthAttachmentRef;
//    if (sampleCount != VK_SAMPLE_COUNT_1_BIT) { subpasses[1].pResolveAttachments = &colorAttachmentResolveRef; }
//    subpasses[1].inputAttachmentCount = 1;
//		subpasses[1].pInputAttachments = &inputReference;
    
    std::array<VkSubpassDependency, 3> dependencies;
    
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;;
		dependencies[0].srcAccessMask = 0;
		dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = 0;

		dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].dstSubpass = 0;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].srcAccessMask = 0;
		dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dependencyFlags = 0;

		dependencies[2].srcSubpass = 0;
		dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    
    std::vector<VkAttachmentDescription> attachmentDescriptions;
    if (sampleCount == VK_SAMPLE_COUNT_1_BIT) {
        attachmentDescriptions = {colorAttachment, depthAttachment};
    } else {
        attachmentDescriptions = {colorAttachment, depthAttachment, colorAttachmentResolve};
    }
    
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
    renderPassInfo.pAttachments = attachmentDescriptions.data();
    renderPassInfo.subpassCount = subpasses.size();
    renderPassInfo.pSubpasses = subpasses.data();
    renderPassInfo.dependencyCount = dependencies.size();
    renderPassInfo.pDependencies = dependencies.data();
    
    if (vkCreateRenderPass(m_Device.device(), &renderPassInfo, nullptr, &attachments.renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
    
    // Framebuffer
    for (int f = 0; f < SwapChain::MAX_FRAMES_IN_FLIGHT; f++) {
        std::vector<VkImageView> imageViewAttachments;
        if (sampleCount == VK_SAMPLE_COUNT_1_BIT) {
            imageViewAttachments = {attachments.color.view[f], attachments.depth.view[f]};
        } else {
            imageViewAttachments = {attachments.multisampling.view[f], attachments.depth.view[f], attachments.color.view[f]};
        }
        
        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = attachments.renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(imageViewAttachments.size());
        framebufferInfo.pAttachments = imageViewAttachments.data();
        framebufferInfo.width = attachments.extent.width;
        framebufferInfo.height = attachments.extent.height;
        framebufferInfo.layers = 1;
        
        if (vkCreateFramebuffer(m_Device.device(), &framebufferInfo, nullptr, &attachments.frameBuffer[f]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
    
    // Image Sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = samplerInfo.addressModeU;
    samplerInfo.addressModeW = samplerInfo.addressModeU;
    
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = m_Device.getPhysicalDeviceProp().limits.maxSamplerAnisotropy;
    
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    
    for (int f = 0; f < SwapChain::MAX_FRAMES_IN_FLIGHT; f++) {
        if (vkCreateSampler(m_Device.device(), &samplerInfo, nullptr, &attachments.sampler[f]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create offscreen sampler!");
        }
        
        attachments.descriptorImage[f] = VkDescriptorImageInfo{
            attachments.sampler[f],
            attachments.color.view[f],
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
    }
    
    // Descriptor set for next Render Pass
    attachments.descriptor.layout = DescriptorSetLayout::Builder(m_Device)
        .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build_ptr();
    
    attachments.descriptor.pool = DescriptorPool::Builder(m_Device)
        .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
        .build_ptr();
    
    attachments.descriptor.v_set.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
        DescriptorWriter(*attachments.descriptor.layout, *attachments.descriptor.pool)
            .writeImage(0, &attachments.descriptorImage[i])
            .build(attachments.descriptor.v_set.at(i));
    }
}


void Renderer::destroyOffscreenPass(RenderPass index) {
    OffscreenPassAttachments& attachments = m_Offscreen[index];
    vkDestroyRenderPass(m_Device.device(), attachments.renderPass, nullptr);
    
    for (int f = 0; f < SwapChain::MAX_FRAMES_IN_FLIGHT; f++) {
        vkDestroySampler(m_Device.device(), attachments.sampler[f], nullptr);
        vkDestroyFramebuffer(m_Device.device(), attachments.frameBuffer[f], nullptr);
        
        vkDestroyImageView(m_Device.device(), attachments.color.view[f], nullptr);
        vkDestroyImage(m_Device.device(), attachments.color.image[f], nullptr);
        vkFreeMemory(m_Device.device(), attachments.color.mem[f], nullptr);
        
        vkDestroyImageView(m_Device.device(), attachments.depth.view[f], nullptr);
        vkDestroyImage(m_Device.device(), attachments.depth.image[f], nullptr);
        vkFreeMemory(m_Device.device(), attachments.depth.mem[f], nullptr);
        
        vkDestroyImageView(m_Device.device(), attachments.multisampling.view[f], nullptr);
        vkDestroyImage(m_Device.device(), attachments.multisampling.image[f], nullptr);
        vkFreeMemory(m_Device.device(), attachments.multisampling.mem[f], nullptr);
    }
}

void Renderer::createDepthPass(RenderPass index) {
    OffscreenPassAttachments& attachments = m_Offscreen[index];
    
    attachments.extent = {2048, 2048};
    
    // Depth Resources
    attachments.depthFormat = m_SwapChain->findDepthFormat();
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = attachments.extent.width;
    imageInfo.extent.height = attachments.extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = attachments.depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = 0;
    
    for (int f = 0; f < SwapChain::MAX_FRAMES_IN_FLIGHT; f++) {
        m_Device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, attachments.depth.image[f], attachments.depth.mem[f]);
        
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = attachments.depth.image[f];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = attachments.depthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(m_Device.device(), &viewInfo, nullptr, &attachments.depth.view[f]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }
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
    samplerInfo.maxAnisotropy = m_Device.getPhysicalDeviceProp().limits.maxSamplerAnisotropy;
    
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    
    
    for (int f = 0; f < SwapChain::MAX_FRAMES_IN_FLIGHT; f++) {
        if (vkCreateSampler(m_Device.device(), &samplerInfo, nullptr, &attachments.sampler[f]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create offscreen sampler!");
        }
        
        attachments.descriptorImage[f] = VkDescriptorImageInfo{
            attachments.sampler[f],
            attachments.depth.view[f],
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        };
    }
    
    // Descriptor set for next Render Pass
    attachments.descriptor.layout = DescriptorSetLayout::Builder(m_Device)
        .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build_ptr();
    
    attachments.descriptor.pool = DescriptorPool::Builder(m_Device)
        .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
        .build_ptr();
    
    attachments.descriptor.v_set.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
        DescriptorWriter(*attachments.descriptor.layout, *attachments.descriptor.pool)
            .writeImage(0, &attachments.descriptorImage[i])
            .build(attachments.descriptor.v_set.at(i));
    }
    
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = attachments.depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    
    VkAttachmentReference depthReference = {};
    depthReference.attachment = 0;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;
    subpass.pDepthStencilAttachment = &depthReference;
    
    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies{};
    
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    
    VkRenderPassCreateInfo renderPassCreateInfo{};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &depthAttachment;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassCreateInfo.pDependencies = dependencies.data();
    
    if (vkCreateRenderPass(m_Device.device(), &renderPassCreateInfo, nullptr, &attachments.renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
    
    // Framebuffer
    for (int f = 0; f < SwapChain::MAX_FRAMES_IN_FLIGHT; f++) {
        
        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = attachments.renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &attachments.depth.view[f];
        framebufferInfo.width = attachments.extent.width;
        framebufferInfo.height = attachments.extent.height;
        framebufferInfo.layers = 1;
        
        if (vkCreateFramebuffer(m_Device.device(), &framebufferInfo, nullptr, &attachments.frameBuffer[f]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void Renderer::destroyDepthPass(RenderPass index) {
    OffscreenPassAttachments& attachments = m_Offscreen[index];
    vkDestroyRenderPass(m_Device.device(), attachments.renderPass, nullptr);
    
    for (int f = 0; f < SwapChain::MAX_FRAMES_IN_FLIGHT; f++) {
        vkDestroySampler(m_Device.device(), attachments.sampler[f], nullptr);
        vkDestroyFramebuffer(m_Device.device(), attachments.frameBuffer[f], nullptr);
        
        vkDestroyImageView(m_Device.device(), attachments.depth.view[f], nullptr);
        vkDestroyImage(m_Device.device(), attachments.depth.image[f], nullptr);
        vkFreeMemory(m_Device.device(), attachments.depth.mem[f], nullptr);
    }
}
