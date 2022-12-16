//
//  UI.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 15/12/22.
//

#include "UI.hpp"

#include <iostream>
#include <fstream>

UI::UI(Device &device) : device{device} {
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameBorderSize = 0.0f;
    style.WindowBorderSize = 0.0f;
    style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_Header] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
}

UI::~UI() {
    ImGui::DestroyContext();
    
    vkDestroyPipeline(device.device(), imguiPipeline, nullptr);
    vkDestroyPipelineLayout(device.device(), imguiPipelineLayout, nullptr);
    
    vkDestroySampler(device.device(), fontSampler, nullptr);
    vkDestroyImageView(device.device(), fontView, nullptr);
    vkDestroyImage(device.device(), fontImage, nullptr);
    vkFreeMemory(device.device(), fontMem, nullptr);
}

void UI::createPipeline(VkDescriptorSetLayout descriptorSetLayout, VkRenderPass renderPass, std::string dynamicShaderPath) {
    VkPushConstantRange pushConstantRanges[1];

    pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRanges[0].offset = 0;
    pushConstantRanges[0].size = sizeof(UI::PushConstBlock);
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
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

void UI::newFrame() {
    ImGui::NewFrame();

    // Init imGui windows and elements

    // SRS - Set initial position of default Debug window (note: Debug window sets its own initial size, use ImGuiSetCond_Always to override)
    ImGui::SetWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetWindowSize(ImVec2(300, 300), ImGuiCond_Always);
    ImGui::TextUnformatted("Vulkan Engine");
    ImGui::TextUnformatted(device.properties.deviceName);
    
    //SRS - Display Vulkan API version and device driver information if available (otherwise blank)
    ImGui::Text("Vulkan API %i.%i.%i", VK_API_VERSION_MAJOR(device.properties.apiVersion), VK_API_VERSION_MINOR(device.properties.apiVersion), VK_API_VERSION_PATCH(device.properties.apiVersion));
    ImGui::Text("%i", device.properties.driverVersion);

    /*
    // Update frame time display
    if (updateFrameGraph) {
        std::rotate(uiSettings.frameTimes.begin(), uiSettings.frameTimes.begin() + 1, uiSettings.frameTimes.end());
        float frameTime = 1000.0f / (example->frameTimer * 1000.0f);
        uiSettings.frameTimes.back() = frameTime;
        if (frameTime < uiSettings.frameTimeMin) {
            uiSettings.frameTimeMin = frameTime;
        }
        if (frameTime > uiSettings.frameTimeMax) {
            uiSettings.frameTimeMax = frameTime;
        }
    }

    ImGui::PlotLines("Frame Times", &uiSettings.frameTimes[0], 50, 0, "", uiSettings.frameTimeMin, uiSettings.frameTimeMax, ImVec2(0, 80));

    ImGui::Text("Camera");
    ImGui::InputFloat3("position", &example->camera.position.x, 2);
    ImGui::InputFloat3("rotation", &example->camera.rotation.x, 2);

    // SRS - Set initial position and size of Example settings window
    ImGui::SetNextWindowPos(ImVec2(20 * example->UIOverlay.scale, 360 * example->UIOverlay.scale), ImGuiSetCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300 * example->UIOverlay.scale, 200 * example->UIOverlay.scale), ImGuiSetCond_FirstUseEver);
    ImGui::Begin("Example settings");
    ImGui::Checkbox("Render models", &uiSettings.displayModels);
    ImGui::Checkbox("Display logos", &uiSettings.displayLogos);
    ImGui::Checkbox("Display background", &uiSettings.displayBackground);
    ImGui::Checkbox("Animate light", &uiSettings.animateLight);
    ImGui::SliderFloat("Light speed", &uiSettings.lightSpeed, 0.1f, 1.0f);
    ImGui::End();
    */

    //SRS - ShowDemoWindow() sets its own initial position and size, cannot override here
    ImGui::ShowDemoWindow();

    // Render to generate draw buffers
    ImGui::Render();
}

void UI::updateBuffers() {
    ImDrawData* imDrawData = ImGui::GetDrawData();

    // Note: Alignment is done inside buffer creation
    VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
    VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

    if ((vertexBufferSize == 0) || (indexBufferSize == 0)) {
        return;
    }

    // Update buffers only if vertex or index count has been changed compared to current buffer size

    // Vertex buffer
    if ((vertexBuffer.getBuffer() == VK_NULL_HANDLE) || (vertexCount != imDrawData->TotalVtxCount)) {
        vertexBuffer.unmap();
        vertexBuffer.destroy();
        vertexBuffer.createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        vertexCount = imDrawData->TotalVtxCount;
        vertexBuffer.map();
    }

    // Index buffer
    if ((indexBuffer.getBuffer() == VK_NULL_HANDLE) || (indexCount < imDrawData->TotalIdxCount)) {
        indexBuffer.unmap();
        indexBuffer.destroy();
        indexBuffer.createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        indexCount = imDrawData->TotalIdxCount;
        indexBuffer.map();
    }
    
    // Upload data
    ImDrawVert* vtxDst = (ImDrawVert*)vertexBuffer.getMappedMemory();
    ImDrawIdx* idxDst = (ImDrawIdx*)indexBuffer.getMappedMemory();

    for (int n = 0; n < imDrawData->CmdListsCount; n++) {
        const ImDrawList* cmd_list = imDrawData->CmdLists[n];
        memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtxDst += cmd_list->VtxBuffer.Size;
        idxDst += cmd_list->IdxBuffer.Size;
    }

    // Flush to make writes visible to GPU
    vertexBuffer.flush();
    indexBuffer.flush();
}


void UI::draw(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet) {
    ImGuiIO& io = ImGui::GetIO();

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, imguiPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
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
        VkBuffer buffers[] = {vertexBuffer.getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT16);

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

VkDescriptorImageInfo UI::loadFontTexture() {
    unsigned char* fontData;
    int texWidth, texHeight;
    
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("fonts/Disket-Mono-Regular.ttf", 24.0f);
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
    
    return VkDescriptorImageInfo {
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
