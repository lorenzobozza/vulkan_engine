//
//  UI.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 15/12/22.
//

#include "include/UI.hpp"

#include <SDL2/SDL.h>

#include <iostream>
#include <fstream>

UI::UI(Device &device, VkRenderPass renderPass, std::string binaryPath) : device{device} {
    vertexBuffers = new std::vector<std::unique_ptr<Buffer>>(SwapChain::MAX_FRAMES_IN_FLIGHT);
    indexBuffers = new std::vector<std::unique_ptr<Buffer>>(SwapChain::MAX_FRAMES_IN_FLIGHT);
    
    for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
        vertexBuffers->at(i) = std::make_unique<Buffer>(device);
        indexBuffers->at(i) = std::make_unique<Buffer>(device);
    }

    ImGui::CreateContext();
    
    loadFontTexture(binaryPath);
    createDescriptors();
    createPipeline(renderPass, binaryPath+"imgui");
    
    ImGui::StyleColorsDark();
}

UI::~UI() {
    ImGui::DestroyContext();
    
    vkDestroyPipeline(device.device(), imguiPipeline, nullptr);
    vkDestroyPipelineLayout(device.device(), imguiPipelineLayout, nullptr);
    
    vkDestroySampler(device.device(), fontSampler, nullptr);
    vkDestroyImageView(device.device(), fontView, nullptr);
    vkDestroyImage(device.device(), fontImage, nullptr);
    vkFreeMemory(device.device(), fontMem, nullptr);
    
    for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
        vertexBuffers->at(i).reset();
        indexBuffers->at(i).reset();
    }
    vertexBuffers = nullptr;
    indexBuffers = nullptr;
}

void UI::createPipeline(VkRenderPass renderPass, std::string dynamicShaderPath) {
    VkPushConstantRange pushConstantRanges[1];

    pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRanges[0].offset = 0;
    pushConstantRanges[0].size = sizeof(UI::PushConstBlock);
    
    auto imguiDescriptorSetLayout = imguiSetLayout->getDescriptorSetLayout();
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &imguiDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges;
    if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &imguiPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }
    
    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = imguiPipelineLayout;
    pipelineConfig.rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    
    auto vertCode = readFile(dynamicShaderPath+".vert.spv");
    auto fragCode = readFile(dynamicShaderPath+".frag.spv");
    
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = vertCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(vertCode.data());
    if(vkCreateShaderModule(device.device(), &createInfo, nullptr, &vertShaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Shader Module");
    }
    createInfo.codeSize = fragCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(fragCode.data());
    if(vkCreateShaderModule(device.device(), &createInfo, nullptr, &fragShaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Shader Module");
    }
    
    VkPipelineShaderStageCreateInfo shaderStages[2];
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertShaderModule;
    shaderStages[0].pName = "main";
    shaderStages[0].flags = 0;
    shaderStages[0].pNext = nullptr;
    shaderStages[0].pSpecializationInfo = nullptr;
    
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragShaderModule;
    shaderStages[1].pName = "main";
    shaderStages[1].flags = 0;
    shaderStages[1].pNext = nullptr;
    shaderStages[1].pSpecializationInfo = nullptr;
    
    VkVertexInputBindingDescription vertexInputBinding = { 0, 20, VK_VERTEX_INPUT_RATE_VERTEX };
    std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
        { 0, 0, VK_FORMAT_R32G32_SFLOAT, 0 },
        { 1, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 2 },
        { 2, 0, VK_FORMAT_R8G8B8A8_UNORM, sizeof(float) * 4 },
    };
    VkPipelineVertexInputStateCreateInfo vertexInputStateCI{};
    vertexInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCI.vertexBindingDescriptionCount = 1;
    vertexInputStateCI.pVertexBindingDescriptions = &vertexInputBinding;
    vertexInputStateCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
    vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputStateCI;
    pipelineInfo.pInputAssemblyState = &pipelineConfig.inputAssemblyInfo;
    pipelineInfo.pViewportState = &pipelineConfig.viewportInfo;
    pipelineInfo.pRasterizationState = &pipelineConfig.rasterizationInfo;
    pipelineInfo.pMultisampleState = &pipelineConfig.multisampleInfo;
    pipelineInfo.pColorBlendState = &pipelineConfig.colorBlendInfo;
    pipelineInfo.pDepthStencilState = &pipelineConfig.depthStencilInfo;
    pipelineInfo.pDynamicState = &pipelineConfig.dynamicStateInfo;
    
    pipelineInfo.layout = pipelineConfig.pipelineLayout;
    pipelineInfo.renderPass = pipelineConfig.renderPass;
    pipelineInfo.subpass = pipelineConfig.subpass;
    
    pipelineInfo.basePipelineIndex = -1;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    
    if(vkCreateGraphicsPipelines(device.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &imguiPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create imgui pipeline");
    }
    
    for (auto shaderStage : shaderStages) {
        vkDestroyShaderModule(device.device(), shaderStage.module, nullptr);
    }
}

void UI::createDescriptors() {
    imguiPool =
       DescriptorPool::Builder(device)
           .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
           .build();
    
    imguiSetLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();

    imguiDescriptorSets = new std::vector<VkDescriptorSet>(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < imguiDescriptorSets->size(); i++) {
        DescriptorWriter(*imguiSetLayout, *imguiPool)
            .writeImage(0, &fontDescriptorInfo)
            .build(imguiDescriptorSets->at(i));
    }
}

void UI::newFrame(Application *app) {
    ImGui::NewFrame();

    // Init imGui windows and elements

    // SRS - Set initial position of default Debug window (note: Debug window sets its own initial size, use ImGuiSetCond_Always to override)
    ImGui::SetWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetWindowSize(ImVec2(300, 300), ImGuiCond_Once);
    ImGui::TextUnformatted("Vulkan Engine");
    //ImGui::TextUnformatted(device.properties.deviceName);
    
    //SRS - Display Vulkan API version and device driver information if available (otherwise blank)
    ImGui::Text("Vulkan API %i.%i.%i", VK_API_VERSION_MAJOR(device.properties.apiVersion), VK_API_VERSION_MINOR(device.properties.apiVersion), VK_API_VERSION_PATCH(device.properties.apiVersion));
    ImGui::Text("%i", device.properties.driverVersion);
    
    app->renderImguiContent();

    //SRS - ShowDemoWindow() sets its own initial position and size, cannot override here
    //ImGui::ShowDemoWindow();

    // Render to generate draw buffers
    ImGui::Render();
}

void UI::updateBuffers(int frameIndex) {
    ImDrawData* imDrawData = ImGui::GetDrawData();

    // Note: Alignment is done inside buffer creation
    VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
    VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

    if ((vertexBufferSize == 0) || (indexBufferSize == 0)) {
        return;
    }

    // Update buffers only if vertex or index count has been changed compared to current buffer size

    // Vertex buffer
    if ((vertexBuffers->at(frameIndex)->getBuffer() == VK_NULL_HANDLE) || (vertexCount[frameIndex] != imDrawData->TotalVtxCount)) {
        vertexBuffers->at(frameIndex)->unmap();
        vertexBuffers->at(frameIndex)->destroy();
        vertexBuffers->at(frameIndex)->createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        vertexCount[frameIndex] = imDrawData->TotalVtxCount;
        vertexBuffers->at(frameIndex)->map();
    }

    // Index buffer
    if ((indexBuffers->at(frameIndex)->getBuffer() == VK_NULL_HANDLE) || (indexCount[frameIndex] < imDrawData->TotalIdxCount)) {
        indexBuffers->at(frameIndex)->unmap();
        indexBuffers->at(frameIndex)->destroy();
        indexBuffers->at(frameIndex)->createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        indexCount[frameIndex] = imDrawData->TotalIdxCount;
        indexBuffers->at(frameIndex)->map();
    }
    
    // Upload data
    ImDrawVert* vtxDst = (ImDrawVert*)vertexBuffers->at(frameIndex)->getMappedMemory();
    ImDrawIdx* idxDst = (ImDrawIdx*)indexBuffers->at(frameIndex)->getMappedMemory();

    for (int n = 0; n < imDrawData->CmdListsCount; n++) {
        const ImDrawList* cmd_list = imDrawData->CmdLists[n];
        memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtxDst += cmd_list->VtxBuffer.Size;
        idxDst += cmd_list->IdxBuffer.Size;
    }

    // Flush to make writes visible to GPU
    vertexBuffers->at(frameIndex)->flush();
    indexBuffers->at(frameIndex)->flush();
}


void UI::draw(VkCommandBuffer commandBuffer, int frameIndex) {
    ImGuiIO& io = ImGui::GetIO();

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, imguiPipelineLayout, 0, 1, &imguiDescriptorSets->at(frameIndex), 0, nullptr);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, imguiPipeline);
    
    VkViewport viewport {
        0, 0,
        ImGui::GetIO().DisplaySize.x,
        ImGui::GetIO().DisplaySize.y,
        0.f,
        1.f
    };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    
    // UI scale and translate via push constants
    pushConstBlock.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
    pushConstBlock.translate = glm::vec2(-1.0f);
    vkCmdPushConstants(commandBuffer, imguiPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UI::PushConstBlock), &pushConstBlock);

    // Render commands
    ImDrawData* imDrawData = ImGui::GetDrawData();
    int32_t vertexOffset = 0;
    int32_t indexOffset = 0;

    if (imDrawData->CmdListsCount > 0) {
        VkBuffer buffers[] = {vertexBuffers->at(frameIndex)->getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffers->at(frameIndex)->getBuffer(), 0, VK_INDEX_TYPE_UINT16);

        for (int32_t i = 0; i < imDrawData->CmdListsCount; i++)
        {
            const ImDrawList* cmd_list = imDrawData->CmdLists[i];
            for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
            {
                const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
                VkRect2D scissorRect;
                scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
                scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
                scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
                scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
                vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);
                vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
                indexOffset += pcmd->ElemCount;
            }
            vertexOffset += cmd_list->VtxBuffer.Size;
        }
    }
}

void UI::loadFontTexture(std::string binaryPath) {
    unsigned char* fontData;
    int texWidth, texHeight;
    
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF((binaryPath+"fonts/monaco.ttf").c_str(), 24.0f);
    io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);

    if (!fontData) {
        throw std::runtime_error("failed to load imgui font data!");
    }
    
    VkDeviceSize imageSize = texWidth * texHeight * 4 * sizeof(char);
    Buffer stagingBuffer{
        device,
        imageSize,
        1,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };
    
    stagingBuffer.map();
    stagingBuffer.writeToBuffer(fontData);
    
    vulkanImage.createImage(
        texWidth, texHeight,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        fontImage,
        fontMem);
    
    auto commandBuffer = vulkanImage.beginSingleTimeCommands();
    vulkanImage.transitionImageLayout(commandBuffer, fontImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vulkanImage.copyBufferToImage(commandBuffer, stagingBuffer.getBuffer(), fontImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    vulkanImage.transitionImageLayout(commandBuffer, fontImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vulkanImage.endSingleTimeCommands(commandBuffer);
    
    fontView = vulkanImage.createImageView(fontImage, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM);
    
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
    
    if (vkCreateSampler(device.device(), &samplerInfo, nullptr, &fontSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create brdf sampler!");
    }
    
    fontDescriptorInfo = VkDescriptorImageInfo {
        fontSampler,
        fontView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
}

std::vector<char> UI::readFile(const std::string &filepath) {
    std::ifstream file{filepath, std::ios::ate | std::ios::binary};
    
    if(!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }
    
    size_t filesize = static_cast<size_t>(file.tellg());
    
    std::vector<char> buffer(filesize);
    file.seekg(0);
    file.read(buffer.data(), filesize);
    file.close();
    
    return buffer ;
}
