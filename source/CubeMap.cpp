//
//  CubeMap.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 30/12/21.
//

#include "CubeMap.hpp"

#include "Camera.hpp"
#include "Primitive.hpp"

#include <array>

CubeMap::CubeMap(const Device& device, VkDescriptorImageInfo* srcDescriptor, VkExtent2D extent, std::string shader, uint16_t mipLevels)
: m_Device(device), m_SrcDescriptor(srcDescriptor), m_Extent(extent), m_ShaderName(shader), m_MipLevels(mipLevels) {
    
    initCubeMap();
    renderFaces();
    
    m_OffscreenPass.descriptor = VkDescriptorImageInfo {
        m_CubeMapSampler,
        m_CubeMap.view,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
}

CubeMap::~CubeMap() {
    vkDestroySampler(m_Device.device(), m_CubeMapSampler, nullptr);
    vkDestroyImageView(m_Device.device(), m_CubeMap.view, nullptr);
    vkDestroyImage(m_Device.device(), m_CubeMap.image, nullptr);
    vkFreeMemory(m_Device.device(), m_CubeMap.mem, nullptr);
    
    freeCommandBuffer();
    
    vkDestroyPipelineLayout(m_Device.device(), m_PipelineLayout, nullptr);
    
    vkDestroyRenderPass(m_Device.device(), m_OffscreenPass.renderPass, nullptr);
}

void CubeMap::initCubeMap(void) {
    createOffscreenRenderPass();
    createDescriptorSets();
    createPipelineLayout();
    createPipeline();
    createCommandBuffer();
}

void CubeMap::destroyFramebuffer(void) {
    vkDestroyFramebuffer(m_Device.device(), m_OffscreenPass.frameBuffer, nullptr);
    
    // Color attachment
    vkDestroyImageView(m_Device.device(), m_OffscreenPass.color.view, nullptr);
    vkDestroyImage(m_Device.device(), m_OffscreenPass.color.image, nullptr);
    vkFreeMemory(m_Device.device(), m_OffscreenPass.color.mem, nullptr);
    
    // Depth attachment
    vkDestroyImageView(m_Device.device(), m_OffscreenPass.depth.view, nullptr);
    vkDestroyImage(m_Device.device(), m_OffscreenPass.depth.image, nullptr);
    vkFreeMemory(m_Device.device(), m_OffscreenPass.depth.mem, nullptr);
}

void CubeMap::renderFaces(void) {
    // Correct mip levels if they exceed the given resolution
    uint16_t maxMip = (uint16_t)std::floor(std::log2(std::max(m_Extent.width, m_Extent.height))) + 1;
    m_MipLevels = std::min(maxMip, m_MipLevels);
    
    Camera cubeCam{};
    cubeCam.setProjection.perspective(1.0f, glm::radians(90.f), .1f, 10.f);
    
    // Create Unit Cube
    Mesh::Data data;
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
    
    std::unique_ptr<Mesh> cubeCanvas = std::make_unique<Mesh>(m_Device, data);
    
    m_Image.createImage(m_Extent.width, m_Extent.height, FB_FORMAT, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_CubeMap.image, m_CubeMap.mem,
                        6,          // Layers
                        m_MipLevels,  // Mip levels
                        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
    
    // Convert final Image to transfer layout
    auto cmbf = m_Image.beginSingleTimeCommands();
    
    m_Image.transitionImageLayout(cmbf, m_CubeMap.image, FB_FORMAT, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  6,          // Layers
                                  m_MipLevels); // Mip levels
    
    m_Image.endSingleTimeCommands(cmbf);
    
    m_CubeMap.view = m_Image.createImageView(m_CubeMap.image, VK_IMAGE_VIEW_TYPE_CUBE, FB_FORMAT, 6, m_MipLevels);
    
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
    
    copyRegion.extent.width = m_Extent.width;
    copyRegion.extent.height = m_Extent.height;
    copyRegion.extent.depth = 1;
    
    //Mip iterator
    for (int mip = 0; mip < m_MipLevels; mip++) {
        m_OffscreenPass.width  = static_cast<int32_t>((float)m_Extent.width * std::pow(0.5, mip));
        m_OffscreenPass.height = static_cast<int32_t>((float)m_Extent.height * std::pow(0.5, mip));
        
        createOffscreenFramebuffer();
        
        //Face Iterator
        for (int i = 0; i < 6; i++){
            cubeCam.setView(lookAtFace(i));
            
            // Render cube face
            beginFrame();
            CubeUbo ubo{};
            ubo.roughness = (float)mip / (float)m_MipLevels;
            ubo.projectionView = cubeCam.getProjection();
            ubo.viewMatrix = cubeCam.getView();
            m_UboBuffer->writeToBuffer(&ubo);
            m_UboBuffer->flush();
            
            beginRenderPass();
            
            m_Pipeline->bind(m_CommandBuffer);
            
            vkCmdBindDescriptorSets(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout,
                                    0, 1, m_Descriptors.v_set.data(), 0, nullptr);
            
            cubeCanvas->bind(m_CommandBuffer);
            cubeCanvas->draw(m_CommandBuffer);
            
            endRenderPass();
            endFrame();
            
            // Copy face to final Image
            auto cmbf = m_Image.beginSingleTimeCommands();
            
            m_Image.transitionImageLayout(cmbf, m_OffscreenPass.color.image, FB_FORMAT,
                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            
            copyRegion.dstSubresource.baseArrayLayer = i;
            copyRegion.dstSubresource.mipLevel = mip;
            copyRegion.extent.width = static_cast<uint32_t>(m_OffscreenPass.width);
            copyRegion.extent.height = static_cast<uint32_t>(m_OffscreenPass.height);
            
            vkCmdCopyImage(cmbf, m_OffscreenPass.color.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           m_CubeMap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
            
            m_Image.transitionImageLayout(cmbf, m_OffscreenPass.color.image, FB_FORMAT,
                                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            
            m_Image.endSingleTimeCommands(cmbf);
        }
        destroyFramebuffer();
        
    }
    
    // Convert final Image to shader layout
    cmbf = m_Image.beginSingleTimeCommands();
    
    m_Image.transitionImageLayout(cmbf, m_CubeMap.image, FB_FORMAT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 6, m_MipLevels);
    
    m_Image.endSingleTimeCommands(cmbf);
    
    // Final Image sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = samplerInfo.addressModeU;
    samplerInfo.addressModeW = samplerInfo.addressModeU;
    
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = m_Device.getPhysicalDeviceProp().limits.maxSamplerAnisotropy;
    
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(m_MipLevels - 1);
    
    if (vkCreateSampler(m_Device.device(), &samplerInfo, nullptr, &m_CubeMapSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
    
}

void CubeMap::createOffscreenRenderPass(void) {
    VkAttachmentDescription osAttachments[2] = {};
    
    m_DepthFormat = m_Device.findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                                                 VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    
    osAttachments[0].format = FB_FORMAT;
    osAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    osAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    osAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    osAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    osAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    osAttachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    osAttachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    // Depth attachment
    osAttachments[1].format = m_DepthFormat;
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
    
    if (vkCreateRenderPass(m_Device.device(), &renderPassCreateInfo, nullptr, &m_OffscreenPass.renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create renderpass!");
    }
}

void CubeMap::createDescriptorSets(void) {
    m_UboBuffer = std::make_unique<Buffer>(m_Device, sizeof(CubeUbo), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    m_UboBuffer->map();
    
    m_Descriptors.layout =
    DescriptorSetLayout::Builder(m_Device)
        .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
        .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build_ptr();
    
    m_Descriptors.pool = DescriptorPool::Builder(m_Device)
        .setMaxSets(1)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
        .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
        .build_ptr();
    
    m_Descriptors.v_set.resize(1);
    auto bufferInfo = m_UboBuffer->descriptorInfo();
    DescriptorWriter(*m_Descriptors.layout, *m_Descriptors.pool)
        .writeBuffer(0, &bufferInfo)
        .writeImage(1, m_SrcDescriptor)
        .build(m_Descriptors.v_set[0]);
}

void CubeMap::createPipelineLayout(void) {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = m_Descriptors.layout->getDescriptorSetLayout();
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;
    if (vkCreatePipelineLayout(m_Device.device(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }
}

void CubeMap::createPipeline(void) {
    assert(m_PipelineLayout != VK_NULL_HANDLE && "Cannot create pipeline before pipeline layout");
    
    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass = m_OffscreenPass.renderPass;
    pipelineConfig.pipelineLayout = m_PipelineLayout;
    pipelineConfig.colorBlendAttachment.blendEnable = VK_FALSE;
    pipelineConfig.rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    
    m_Pipeline = std::make_unique<Pipeline>(m_Device, "cubemap.vert", m_ShaderName+".frag", pipelineConfig);
}

void CubeMap::createOffscreenFramebuffer(void) {
    m_Image.createImage(m_OffscreenPass.width, m_OffscreenPass.height,
                        FB_FORMAT,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        m_OffscreenPass.color.image,
                        m_OffscreenPass.color.mem);
    
    m_Image.createImage(m_OffscreenPass.width, m_OffscreenPass.height,
                        m_DepthFormat,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        m_OffscreenPass.depth.image,
                        m_OffscreenPass.depth.mem);
    
    auto commandBuffer = m_Image.beginSingleTimeCommands();
    
    m_Image.transitionImageLayout(commandBuffer,
                                  m_OffscreenPass.color.image,
                                  FB_FORMAT,
                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    
    m_Image.transitionImageLayout(commandBuffer,
                                  m_OffscreenPass.depth.image,
                                  m_DepthFormat,
                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                  1,  // Layers
                                  1,  // Mip levels
                                  0,  // Base mip
                                  VK_IMAGE_ASPECT_DEPTH_BIT);
    
    m_OffscreenPass.color.view = m_Image.createImageView(m_OffscreenPass.color.image, VK_IMAGE_VIEW_TYPE_2D, FB_FORMAT);
    
    m_OffscreenPass.depth.view = m_Image.createImageView(m_OffscreenPass.depth.image, VK_IMAGE_VIEW_TYPE_2D, m_DepthFormat, 1,
                                                         1, VK_IMAGE_ASPECT_DEPTH_BIT);
    
    m_Image.endSingleTimeCommands(commandBuffer);
    
    VkImageView attachments[2];
    attachments[0] = m_OffscreenPass.color.view;
    attachments[1] = m_OffscreenPass.depth.view;
    
    VkFramebufferCreateInfo fbufCreateInfo{};
    fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbufCreateInfo.renderPass = m_OffscreenPass.renderPass;
    fbufCreateInfo.attachmentCount = 2;
    fbufCreateInfo.pAttachments = attachments;
    fbufCreateInfo.width = m_OffscreenPass.width;
    fbufCreateInfo.height = m_OffscreenPass.height;
    fbufCreateInfo.layers = 1;
    
    if (vkCreateFramebuffer(m_Device.device(), &fbufCreateInfo, nullptr, &m_OffscreenPass.frameBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create framebuffer!");
    }
    
}

VkCommandBuffer CubeMap::beginFrame(void) {
    assert(!m_IsFrameStarted && "Can't call beginFrame while already in progress");
    
    m_IsFrameStarted = true;
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    if (vkBeginCommandBuffer(m_CommandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffers");
    }
    
    return m_CommandBuffer;
}

void CubeMap::endFrame(void) {
    assert(m_IsFrameStarted && "Can't call endFrame while frame is not in progress");
    
    if (vkEndCommandBuffer(m_CommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
    }
    
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.pWaitDstStageMask = waitStages;
    
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_CommandBuffer;
    
    if (vkQueueSubmit(m_Device.transferQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }
    
    vkQueueWaitIdle(m_Device.transferQueue());
    
    m_IsFrameStarted = false;
}

void CubeMap::beginRenderPass(void) {
    assert(m_IsFrameStarted && "Can't call endFrame while frame is not in progress");
    
    VkRenderPassBeginInfo renderpassInfo{};
    renderpassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderpassInfo.renderPass = m_OffscreenPass.renderPass;
    renderpassInfo.framebuffer = m_OffscreenPass.frameBuffer;
    
    renderpassInfo.renderArea.offset = {0, 0};
    renderpassInfo.renderArea.extent.width = m_OffscreenPass.width;
    renderpassInfo.renderArea.extent.height = m_OffscreenPass.height;
    
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    renderpassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderpassInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(m_CommandBuffer, &renderpassInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_OffscreenPass.width);
    viewport.height = static_cast<float>(m_OffscreenPass.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, m_Extent};
    vkCmdSetViewport(m_CommandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(m_CommandBuffer, 0, 1, &scissor);
}

void CubeMap::endRenderPass(void) {
    assert(m_IsFrameStarted && "Can't call endFrame while frame is not in progress");
    
    vkCmdEndRenderPass(m_CommandBuffer);
}

void CubeMap::createCommandBuffer(void) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_Device.getTransferCommandPool();
    allocInfo.commandBufferCount = 1;
    
    if (vkAllocateCommandBuffers(m_Device.device(), &allocInfo, &m_CommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }
}

void CubeMap::freeCommandBuffer(void) {
    vkFreeCommandBuffers(m_Device.device(), m_Device.getTransferCommandPool(), 1, &m_CommandBuffer);
    m_CommandBuffer = VK_NULL_HANDLE;
}

glm::mat4 CubeMap::lookAtFace(const uint16_t index) {
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
