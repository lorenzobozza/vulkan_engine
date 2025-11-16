//
//  BRDF.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 27/10/25.
//

#include "Renderer.hpp"
#include <array>

static const VkExtent2D lutExtent = {256, 256};

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
        "brdf.vert",
        "brdf.frag",
        pipelineConfig
    };
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = lutExtent.width;
    imageInfo.extent.height = lutExtent.height;
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
    fbufCreateInfo.width = lutExtent.width;
    fbufCreateInfo.height = lutExtent.height;
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
    allocInfo.commandPool = device.getTransferCommandPool();
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
    renderpassInfo.renderArea.extent = lutExtent;
    
    VkClearValue clearValue{};
    clearValue.color = {0.f, 0.f, 0.f, 0.f};
    renderpassInfo.clearValueCount = 1;
    renderpassInfo.pClearValues = &clearValue;
    
    vkCmdBeginRenderPass(commandBuffer, &renderpassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = lutExtent.width;
    viewport.height = lutExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, lutExtent};
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
    
    if (vkQueueSubmit(device.transferQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }
    
    vkQueueWaitIdle(device.transferQueue());
    
    vkFreeCommandBuffers(
        device.device(),
        device.getTransferCommandPool(),
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
    
    wasBrdfRequested = true;
}

void Renderer::destroyBrdfLut(void) {
    if (wasBrdfRequested) {
        vkDestroySampler(device.device(), brdfSampler, nullptr);
        vkDestroyImageView(device.device(), brdf.view, nullptr);
        vkDestroyImage(device.device(), brdf.image, nullptr);
        vkFreeMemory(device.device(), brdf.mem, nullptr);
    }
}
