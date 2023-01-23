//
//  HDRi.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 30/12/21.
//

#include "include/HDRi.hpp"

#include <array>

HDRi::HDRi(Device &device, VkDescriptorImageInfo &srcDescriptor, VkExtent2D extent, std::string shader, std::string binaryPath, uint16_t mipLevels)
    : device{device}, srcDescriptor{srcDescriptor}, extent{extent}, shader{shader}, binaryPath{binaryPath}, mipLevels{mipLevels} {
    
    initHDRi();
    renderFaces();
}

HDRi::~HDRi() {
    vkDestroySampler(device.device(), cubeSampler, nullptr);
    vkDestroyImageView(device.device(), cubeMap.view, nullptr);
    vkDestroyImage(device.device(), cubeMap.image, nullptr);
    vkFreeMemory(device.device(), cubeMap.mem, nullptr);

    freeCommandBuffer();
    
    vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
    
    vkDestroyRenderPass(device.device(), offscreenPass.renderPass, nullptr);
}

void HDRi::initHDRi() {
    createOffscreenRenderPass();
    createDescriptorSets();
    createPipelineLayout();
    createPipeline();
    createCommandBuffer();
}

void HDRi::destroyFramebuffer() {
    vkDestroyFramebuffer(device.device(), offscreenPass.frameBuffer, nullptr);
    
    // Color attachment
    vkDestroyImageView(device.device(), offscreenPass.color.view, nullptr);
    vkDestroyImage(device.device(), offscreenPass.color.image, nullptr);
    vkFreeMemory(device.device(), offscreenPass.color.mem, nullptr);

    // Depth attachment
    vkDestroyImageView(device.device(), offscreenPass.depth.view, nullptr);
    vkDestroyImage(device.device(), offscreenPass.depth.image, nullptr);
    vkFreeMemory(device.device(), offscreenPass.depth.mem, nullptr);
}

VkDescriptorImageInfo HDRi::descriptorInfo() {
    return VkDescriptorImageInfo {
        cubeSampler,
        cubeMap.view,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
}

void HDRi::renderFaces() {    
    
    Camera cubeCam{};
    cubeCam.setProjection.perspective(1.0f, glm::radians(90.f), .1f, 10.f);
    
    // Create Unit Cube
    Model::Data data;
    data.vertices = {
        {{-.5f, -.5f, .5f}},
        {{.5f, -.5f, .5f}},
        {{.5f, .5f, .5f}},
        {{-.5f, .5f, .5f}},
        {{-.5f, -.5f, -.5f}},
        {{.5f, -.5f, -.5f}},
        {{.5f, .5f, -.5f}},
        {{-.5f, .5f, -.5f}}
    };
    data.indices = {
        0,1,2,2,3,0,
        4,5,1,1,0,4,
        4,0,3,3,7,4,
        1,5,6,6,2,1,
        3,2,6,6,7,3,
        5,4,7,7,6,5
    };
    
    auto cube = SolidObject::createSolidObject();
    cube.model = std::make_unique<Model>(device, data);
    
    SolidObject::Map cubeEnvironment;
    cubeEnvironment.emplace(cube.getId(), std::move(cube));

    vulkanImage.createImage(
        extent.width, extent.height,
        FB_FORMAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        cubeMap.image, cubeMap.mem,
        6,          // Layers
        mipLevels,  // Mip levels
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
    
    // Convert final Image to transfer layout
    auto cmbf = vulkanImage.beginSingleTimeCommands();
        
    vulkanImage.transitionImageLayout(
        cmbf,
        cubeMap.image,
        FB_FORMAT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        6,          // Layers
        mipLevels); // Mip levels
        
    vulkanImage.endSingleTimeCommands(cmbf);
    
    cubeMap.view = vulkanImage.createImageView(cubeMap.image, VK_IMAGE_VIEW_TYPE_CUBE, FB_FORMAT, 6, mipLevels);
    
    VkImageCopy copyRegion = {};

    copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.baseArrayLayer = 0;
    copyRegion.srcSubresource.mipLevel = 0;
    copyRegion.srcSubresource.layerCount = 1;
    copyRegion.srcOffset = { 0, 0, 0 };

    copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.dstSubresource.baseArrayLayer = 0;
    copyRegion.dstSubresource.mipLevel = 0;
    copyRegion.dstSubresource.layerCount = 1;
    copyRegion.dstOffset = { 0, 0, 0 };
    
    copyRegion.extent.width = extent.width;
    copyRegion.extent.height = extent.height;
    copyRegion.extent.depth = 1;
    
    //Mip iterator
    for (int mip = 0; mip < mipLevels; mip++) {
        offscreenPass.width  = static_cast<float>(extent.width * std::pow(0.5, mip));
        offscreenPass.height = static_cast<float>(extent.height * std::pow(0.5, mip));
        
        createOffscreenFramebuffer();

       //Face Iterator
        for (int i = 0; i < 6; i++){
            cubeCam.setView(lookAtFace(i));
            
            // Render cube face
            beginFrame();
            CubeUbo ubo{};
                ubo.roughness = (float)mip / (float)mipLevels;
                ubo.projectionView = cubeCam.getProjection();
                ubo.viewMatrix = cubeCam.getView();
                uboBuffer->writeToBuffer(&ubo);
                uboBuffer->flush();
                
            beginRenderPass();
            
            pipeline->bind(commandBuffer);
              
            vkCmdBindDescriptorSets(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout,
                0,
                1,
                &descriptor.set,
                0,
                nullptr
            );
            
            cubeEnvironment.at(cube.getId()).model->bind(commandBuffer);
            cubeEnvironment.at(cube.getId()).model->draw(commandBuffer);
            
            endRenderPass();
            endFrame();
            
            // Copy face to final Image
            auto cmbf = vulkanImage.beginSingleTimeCommands();
            
            vulkanImage.transitionImageLayout(
                cmbf,
                offscreenPass.color.image,
                FB_FORMAT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            copyRegion.dstSubresource.baseArrayLayer = i;
            copyRegion.dstSubresource.mipLevel = mip;
            copyRegion.extent.width = static_cast<uint32_t>(offscreenPass.width);
            copyRegion.extent.height = static_cast<uint32_t>(offscreenPass.height);
      
            vkCmdCopyImage(
                cmbf,
                offscreenPass.color.image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                cubeMap.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copyRegion);
      
            vulkanImage.transitionImageLayout(
                cmbf,
                offscreenPass.color.image,
                FB_FORMAT,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            
            vulkanImage.endSingleTimeCommands(cmbf);
        }
        destroyFramebuffer();
        
    }
    
    // Convert final Image to shader layout
    cmbf = vulkanImage.beginSingleTimeCommands();
        
    vulkanImage.transitionImageLayout(
        cmbf,
        cubeMap.image,
        FB_FORMAT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        6,
        mipLevels);
        
    vulkanImage.endSingleTimeCommands(cmbf);
    
    // Final Image sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = samplerInfo.addressModeU;
    samplerInfo.addressModeW = samplerInfo.addressModeU;
    
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = device.properties.limits.maxSamplerAnisotropy;
    
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(mipLevels - 1);
    
    if (vkCreateSampler(device.device(), &samplerInfo, nullptr, &cubeSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
    
}

void HDRi::createOffscreenRenderPass() {
    VkAttachmentDescription osAttachments[2] = {};
    
    depthFormat = device.findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    osAttachments[0].format = FB_FORMAT;
    osAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    osAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    osAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    osAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    osAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    osAttachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    osAttachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth attachment
    osAttachments[1].format = depthFormat;
    osAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    osAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    osAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    osAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    osAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    osAttachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    osAttachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorReference = {};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference = {};
    depthReference.attachment = 1;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;
    subpass.pDepthStencilAttachment = &depthReference;

    VkRenderPassCreateInfo renderPassCreateInfo{};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = 2;
    renderPassCreateInfo.pAttachments = osAttachments;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(device.device(), &renderPassCreateInfo, nullptr, &offscreenPass.renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create renderpass!");
    }
}

void HDRi::createDescriptorSets() {
    uboBuffer = std::make_unique<Buffer>(
        device,
        sizeof(CubeUbo),
        1,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
    );
    uboBuffer->map();
    
    descriptor.pool =
       DescriptorPool::Builder(device)
           .setMaxSets(1)
           .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
           .build();
           
    descriptor.setLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
    
    auto bufferInfo = uboBuffer->descriptorInfo();
    DescriptorWriter(*descriptor.setLayout, *descriptor.pool)
        .writeBuffer(0, &bufferInfo)
        .writeImage(1, &srcDescriptor)
        .build(descriptor.set);
}

void HDRi::createPipelineLayout() {
  std::vector<VkDescriptorSetLayout> descriptorSetLayouts{descriptor.setLayout->getDescriptorSetLayout()};

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = 0;
  pipelineLayoutInfo.pPushConstantRanges = nullptr;
  if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }
}

void HDRi::createPipeline() {
  assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

  PipelineConfigInfo pipelineConfig{};
  Pipeline::defaultPipelineConfigInfo(pipelineConfig);
  pipelineConfig.renderPass = offscreenPass.renderPass;
  pipelineConfig.pipelineLayout = pipelineLayout;
  pipelineConfig.colorBlendAttachment.blendEnable = VK_FALSE;
  pipelineConfig.rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
  pipeline = std::make_unique<Pipeline>(
      device,
      binaryPath+"cubemap.vert.spv",
      binaryPath+shader+".frag.spv",
      pipelineConfig);
}

void HDRi::createOffscreenFramebuffer() {
    vulkanImage.createImage(
        offscreenPass.width, offscreenPass.height,
        FB_FORMAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        offscreenPass.color.image,
        offscreenPass.color.mem);
        
    vulkanImage.createImage(
        offscreenPass.width, offscreenPass.height,
        depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        offscreenPass.depth.image,
        offscreenPass.depth.mem);

    auto commandBuffer = vulkanImage.beginSingleTimeCommands();
    
    vulkanImage.transitionImageLayout(
        commandBuffer,
        offscreenPass.color.image,
        FB_FORMAT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    
    vulkanImage.transitionImageLayout(
        commandBuffer,
        offscreenPass.depth.image,
        depthFormat,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        1,  // Layers
        1,  // Mip levels
        VK_IMAGE_ASPECT_DEPTH_BIT);
    
    offscreenPass.color.view = vulkanImage.createImageView(
        offscreenPass.color.image,
        VK_IMAGE_VIEW_TYPE_2D,
        FB_FORMAT);
        
    offscreenPass.depth.view = vulkanImage.createImageView(
        offscreenPass.depth.image,
        VK_IMAGE_VIEW_TYPE_2D,
        depthFormat,
        1,  // Layers
        1,  // Mip levels
        VK_IMAGE_ASPECT_DEPTH_BIT);
        
    vulkanImage.endSingleTimeCommands(commandBuffer);

    VkImageView attachments[2];
    attachments[0] = offscreenPass.color.view;
    attachments[1] = offscreenPass.depth.view;
    
    VkFramebufferCreateInfo fbufCreateInfo{};
    fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbufCreateInfo.renderPass = offscreenPass.renderPass;
    fbufCreateInfo.attachmentCount = 2;
    fbufCreateInfo.pAttachments = attachments;
    fbufCreateInfo.width = offscreenPass.width;
    fbufCreateInfo.height = offscreenPass.height;
    fbufCreateInfo.layers = 1;

	if (vkCreateFramebuffer(device.device(), &fbufCreateInfo, nullptr, &offscreenPass.frameBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create framebuffer!");
    }
    
}

VkCommandBuffer HDRi::beginFrame() {
    assert(!isFrameStarted && "Can't call beginFrame while already in progress");
    
    isFrameStarted = true;
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffers");
    }
    
    return commandBuffer;
}

void HDRi::endFrame() {
    assert(isFrameStarted && "Can't call endFrame while frame is not in progress");

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
    
    isFrameStarted = false;
}
    
void HDRi::beginRenderPass() {
    assert(isFrameStarted && "Can't call endFrame while frame is not in progress");
    
    VkRenderPassBeginInfo renderpassInfo{};
    renderpassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderpassInfo.renderPass = offscreenPass.renderPass;
    renderpassInfo.framebuffer = offscreenPass.frameBuffer;
    
    renderpassInfo.renderArea.offset = {0, 0};
    renderpassInfo.renderArea.extent.width = offscreenPass.width;
    renderpassInfo.renderArea.extent.height = offscreenPass.height;
    
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    renderpassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderpassInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(commandBuffer, &renderpassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(offscreenPass.width);
    viewport.height = static_cast<float>(offscreenPass.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void HDRi::endRenderPass() {
    assert(isFrameStarted && "Can't call endFrame while frame is not in progress");

    vkCmdEndRenderPass(commandBuffer);
}

void HDRi::createCommandBuffer() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = device.getCommandPool();
    allocInfo.commandBufferCount = 1;
    
    if (vkAllocateCommandBuffers(device.device(), &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }
}

void HDRi::freeCommandBuffer() {
    vkFreeCommandBuffers(
        device.device(),
        device.getCommandPool(),
        1,
        &commandBuffer
    );
    commandBuffer = VK_NULL_HANDLE;
}

glm::mat4 HDRi::lookAtFace(const uint16_t index) {
    glm::mat4 viewMatrix = glm::mat4{1.f};
    switch (index) {
        case 0: // POSITIVE_X
			viewMatrix = glm::rotate(viewMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			break;
		case 1:	// NEGATIVE_X
			viewMatrix = glm::rotate(viewMatrix, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			break;
		case 2:	// POSITIVE_Y
			viewMatrix = glm::rotate(viewMatrix, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			break;
		case 3:	// NEGATIVE_Y
			viewMatrix = glm::rotate(viewMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			break;
		case 4:	// POSITIVE_Z
			viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			break;
		case 5:	// NEGATIVE_Z
			viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			break;
    }
    return viewMatrix;
}
