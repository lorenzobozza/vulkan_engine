//
//  UI.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 15/12/22.
//

#include "UI.hpp"
#include "Pipeline.hpp"
#include "ImGuiShaders.h"
#include "Log.hpp"

#include <SDL2/SDL.h>
#include <imgui_internal.h>

#include <fstream>

UI::UI(const Device& device, Renderer& renderer) : m_Device{device}, m_Renderer{renderer} {
    m_VertexBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    m_IndexBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    
    for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
        m_VertexBuffers.at(i) = std::make_unique<Buffer>(m_Device);
        m_IndexBuffers.at(i) = std::make_unique<Buffer>(m_Device);
    }
    
    m_ImGuiContext = ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    loadFontTexture();
    createDescriptors();
    createPipeline(m_Renderer.getSwapChainRenderPass(), "imgui");
}

UI::~UI() {
    ImGui::DestroyContext();
    
    vkDestroyPipeline(m_Device.device(), m_ImguiPipeline, nullptr);
    vkDestroyPipelineLayout(m_Device.device(), m_ImguiPipelineLayout, nullptr);
    
    vkDestroySampler(m_Device.device(), m_FontSampler, nullptr);
    vkDestroyImageView(m_Device.device(), m_FontView, nullptr);
    vkDestroyImage(m_Device.device(), m_FontImage, nullptr);
    vkFreeMemory(m_Device.device(), m_FontMem, nullptr);
}

void UI::createPipeline(VkRenderPass renderPass, std::string dynamicShaderPath) {
    VkPushConstantRange pushConstantRanges[1];
    
    pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRanges[0].offset = 0;
    pushConstantRanges[0].size = sizeof(UI::PushConstBlock);
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = m_Descriptor.layout->getDescriptorSetLayout();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges;
    if (vkCreatePipelineLayout(m_Device.device(), &pipelineLayoutInfo, nullptr, &m_ImguiPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }
    
    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = m_ImguiPipelineLayout;
    pipelineConfig.rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    
    //    std::vector<uint32_t> vertexShader, fragmentShader;
    
    //    ShaderCompiler glslc;
    
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    //    if (glslc.loadShader("imgui.vert", vertexShader) != ShaderCompiler::State::Valid) {
    //        throw std::runtime_error("Failed loading basic UI shader");
    //    }
    //    createInfo.codeSize = vertexShader.size() * sizeof(uint32_t);
    //    createInfo.pCode = vertexShader.data();
    createInfo.codeSize = imgui_vert_spv_len;
    createInfo.pCode = reinterpret_cast<const uint32_t*>(imgui_vert_spv);
    if(vkCreateShaderModule(m_Device.device(), &createInfo, nullptr, &m_VertShaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Shader Module");
    }
    
    
    //    if (glslc.loadShader("imgui.frag", fragmentShader) != ShaderCompiler::State::Valid) {
    //        throw std::runtime_error("Failed loading basic UI shader");
    //    }
    //    createInfo.codeSize = fragmentShader.size() * sizeof(uint32_t);
    //    createInfo.pCode = fragmentShader.data();
    createInfo.codeSize = imgui_frag_spv_len;
    createInfo.pCode = reinterpret_cast<const uint32_t*>(imgui_frag_spv);
    if(vkCreateShaderModule(m_Device.device(), &createInfo, nullptr, &m_FragShaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Shader Module");
    }
    
    VkPipelineShaderStageCreateInfo shaderStages[2];
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = m_VertShaderModule;
    shaderStages[0].pName = "main";
    shaderStages[0].flags = 0;
    shaderStages[0].pNext = nullptr;
    shaderStages[0].pSpecializationInfo = nullptr;
    
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = m_FragShaderModule;
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
    
    if(vkCreateGraphicsPipelines(m_Device.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_ImguiPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create imgui pipeline");
    }
    
    for (auto shaderStage : shaderStages) {
        vkDestroyShaderModule(m_Device.device(), shaderStage.module, nullptr);
    }
}

void UI::createDescriptors(void) {
    m_Descriptor.layout = DescriptorSetLayout::Builder(m_Device)
        .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build_ptr();
    
    m_Descriptor.pool = DescriptorPool::Builder(m_Device)
        .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
        .build_ptr();
    
    m_Descriptor.v_set.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
        DescriptorWriter(*m_Descriptor.layout, *m_Descriptor.pool)
            .writeImage(0, &m_FontDescriptorInfo)
            .build(m_Descriptor.v_set.at(i));
    }
}

void UI::newFrame(void) {
    ImGui::NewFrame();
    
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags windowFlags =
    ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    ImGui::Begin("DockSpace", nullptr, windowFlags);
    
    const auto mainDockspaceId = ImGui::GetID("MainDockspace");
    ImGui::DockSpace(mainDockspaceId);
    
    
    static bool firstLoop = true;
    if (firstLoop) {
        
        ImGuiID id = ImGui::GetID("MainDockspace");
        
        ImGui::DockBuilderRemoveNode(mainDockspaceId);
        ImGui::DockBuilderAddNode(mainDockspaceId, ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_PassthruCentralNode);
        
        ImGuiID dock1 = ImGui::DockBuilderSplitNode(id, ImGuiDir_Right, 0.8f, nullptr, &id);
        
        ImGuiID dock2 = ImGui::DockBuilderSplitNode(id, ImGuiDir_Left, 0.2f, nullptr, &id);
        
        ImGuiID dock3 = ImGui::DockBuilderSplitNode(dock1, ImGuiDir_Down, 0.2f, nullptr, &dock1);
        
        ImGuiID dock4 = ImGui::DockBuilderSplitNode(dock2, ImGuiDir_Down, 0.37f, nullptr, &dock2);
        
        
        ImGui::DockBuilderDockWindow("Viewport", dock1);
        ImGui::DockBuilderDockWindow("Settings", dock2);
        ImGui::DockBuilderDockWindow("Log Console", dock3);
        ImGui::DockBuilderDockWindow("Assets", dock4);
        ImGui::DockBuilderDockWindow("Node Visualizer", dock4);
        ImGui::DockBuilderDockWindow("Materials", dock1);
        
        
        ImGui::DockBuilderFinish(id);
        
        firstLoop = false;
    }
    
    for (auto& widget : m_Widgets) {
        widget->draw();
    }
    
    ImGui::End(); // "DockSpace"
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
    if ((m_VertexBuffers.at(frameIndex)->getBuffer() == VK_NULL_HANDLE) || (m_VertexCount[frameIndex] != imDrawData->TotalVtxCount)) {
        m_VertexBuffers.at(frameIndex)->unmap();
        m_VertexBuffers.at(frameIndex)->destroy();
        m_VertexBuffers.at(frameIndex)->createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        m_VertexCount[frameIndex] = imDrawData->TotalVtxCount;
        m_VertexBuffers.at(frameIndex)->map();
    }
    
    // Index buffer
    if ((m_IndexBuffers.at(frameIndex)->getBuffer() == VK_NULL_HANDLE) || (m_IndexCount[frameIndex] < imDrawData->TotalIdxCount)) {
        m_IndexBuffers.at(frameIndex)->unmap();
        m_IndexBuffers.at(frameIndex)->destroy();
        m_IndexBuffers.at(frameIndex)->createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        m_IndexCount[frameIndex] = imDrawData->TotalIdxCount;
        m_IndexBuffers.at(frameIndex)->map();
    }
    
    // Upload data
    ImDrawVert* vtxDst = (ImDrawVert*)m_VertexBuffers.at(frameIndex)->getMappedMemory();
    ImDrawIdx* idxDst = (ImDrawIdx*)m_IndexBuffers.at(frameIndex)->getMappedMemory();
    
    for (int n = 0; n < imDrawData->CmdListsCount; n++) {
        const ImDrawList* cmd_list = imDrawData->CmdLists[n];
        memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtxDst += cmd_list->VtxBuffer.Size;
        idxDst += cmd_list->IdxBuffer.Size;
    }
    
    // Flush to make writes visible to GPU
    m_VertexBuffers.at(frameIndex)->flush();
    m_IndexBuffers.at(frameIndex)->flush();
}


void UI::draw(VkCommandBuffer commandBuffer, int frameIndex) {
    ImGuiIO& io = ImGui::GetIO();
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ImguiPipeline);
    
    VkViewport viewport {
        0, 0,
        ImGui::GetIO().DisplaySize.x,
        ImGui::GetIO().DisplaySize.y,
        0.f,
        1.f
    };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    
    // UI scale and translate via push constants
    m_PushConstBlock.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
    m_PushConstBlock.translate = glm::vec2(-1.0f);
    vkCmdPushConstants(commandBuffer, m_ImguiPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UI::PushConstBlock), &m_PushConstBlock);
    
    // Render commands
    ImDrawData* imDrawData = ImGui::GetDrawData();
    int32_t vertexOffset = 0;
    int32_t indexOffset = 0;
    
    if (imDrawData->CmdListsCount > 0) {
        VkBuffer buffers[] = {m_VertexBuffers.at(frameIndex)->getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffers.at(frameIndex)->getBuffer(), 0, VK_INDEX_TYPE_UINT16);
        
        for (int32_t i = 0; i < imDrawData->CmdListsCount; i++) {
            const ImDrawList* cmd_list = imDrawData->CmdLists[i];
            for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++) {
                const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
                
                VkDescriptorSet dSet;
                if (pcmd->TexRef._TexData) {
                    dSet = m_Descriptor.v_set.at(frameIndex);
                } else {
                    dSet = m_Renderer.getDescriptorSets((RenderPass)pcmd->TexRef._TexID)->at(frameIndex);
                }
                
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ImguiPipelineLayout, 0, 1, &dSet, 0, nullptr);
                
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

void UI::loadFontTexture(void) {
    unsigned char* fontData;
    int texWidth, texHeight;
    
    ImGuiIO &io = ImGui::GetIO();
    ImFontConfig c{};
    c.OversampleH = 4;
    c.OversampleV = 4;
    io.Fonts->AddFontFromFileTTF("../../../assets/fonts/Inter.ttf", 20.f, &c);
    io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
    
    if (!fontData) {
        throw std::runtime_error("failed to load imgui font data!");
    }
    
    VkDeviceSize imageSize = texWidth * texHeight * 4 * sizeof(char);
    Buffer stagingBuffer{
        m_Device,
        imageSize,
        1,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };
    
    stagingBuffer.map();
    stagingBuffer.writeToBuffer(fontData);
    
    m_Image.createImage(texWidth, texHeight,
                        VK_FORMAT_R8G8B8A8_UNORM,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        m_FontImage,
                        m_FontMem);
    
    auto commandBuffer = m_Image.beginSingleTimeCommands();
    m_Image.transitionImageLayout(commandBuffer, m_FontImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    m_Image.copyBufferToImage(commandBuffer, stagingBuffer.getBuffer(), m_FontImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    m_Image.transitionImageLayout(commandBuffer, m_FontImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_Image.endSingleTimeCommands(commandBuffer);
    
    m_FontView = m_Image.createImageView(m_FontImage, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM);
    
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
    
    if (vkCreateSampler(m_Device.device(), &samplerInfo, nullptr, &m_FontSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create brdf sampler!");
    }
    
    m_FontDescriptorInfo = VkDescriptorImageInfo {
        m_FontSampler,
        m_FontView,
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

ImGuiKey UI::ImGui_SDL2_KeyEventToImGuiKey(SDL_Keycode keycode)
{
    switch (keycode)
        {
        case SDLK_TAB: return ImGuiKey_Tab;
        case SDLK_LEFT: return ImGuiKey_LeftArrow;
        case SDLK_RIGHT: return ImGuiKey_RightArrow;
        case SDLK_UP: return ImGuiKey_UpArrow;
        case SDLK_DOWN: return ImGuiKey_DownArrow;
        case SDLK_PAGEUP: return ImGuiKey_PageUp;
        case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
        case SDLK_HOME: return ImGuiKey_Home;
        case SDLK_END: return ImGuiKey_End;
        case SDLK_INSERT: return ImGuiKey_Insert;
        case SDLK_DELETE: return ImGuiKey_Delete;
        case SDLK_BACKSPACE: return ImGuiKey_Backspace;
        case SDLK_SPACE: return ImGuiKey_Space;
        case SDLK_RETURN: return ImGuiKey_Enter;
        case SDLK_ESCAPE: return ImGuiKey_Escape;
        case SDLK_QUOTE: return ImGuiKey_Apostrophe;
        case SDLK_COMMA: return ImGuiKey_Comma;
        case SDLK_MINUS: return ImGuiKey_Minus;
        case SDLK_PERIOD: return ImGuiKey_Period;
        case SDLK_SLASH: return ImGuiKey_Slash;
        case SDLK_SEMICOLON: return ImGuiKey_Semicolon;
        case SDLK_EQUALS: return ImGuiKey_Equal;
        case SDLK_LEFTBRACKET: return ImGuiKey_LeftBracket;
        case SDLK_BACKSLASH: return ImGuiKey_Backslash;
        case SDLK_RIGHTBRACKET: return ImGuiKey_RightBracket;
        case SDLK_BACKQUOTE: return ImGuiKey_GraveAccent;
        case SDLK_CAPSLOCK: return ImGuiKey_CapsLock;
        case SDLK_SCROLLLOCK: return ImGuiKey_ScrollLock;
        case SDLK_NUMLOCKCLEAR: return ImGuiKey_NumLock;
        case SDLK_PRINTSCREEN: return ImGuiKey_PrintScreen;
        case SDLK_PAUSE: return ImGuiKey_Pause;
        case SDLK_KP_0: return ImGuiKey_Keypad0;
        case SDLK_KP_1: return ImGuiKey_Keypad1;
        case SDLK_KP_2: return ImGuiKey_Keypad2;
        case SDLK_KP_3: return ImGuiKey_Keypad3;
        case SDLK_KP_4: return ImGuiKey_Keypad4;
        case SDLK_KP_5: return ImGuiKey_Keypad5;
        case SDLK_KP_6: return ImGuiKey_Keypad6;
        case SDLK_KP_7: return ImGuiKey_Keypad7;
        case SDLK_KP_8: return ImGuiKey_Keypad8;
        case SDLK_KP_9: return ImGuiKey_Keypad9;
        case SDLK_KP_PERIOD: return ImGuiKey_KeypadDecimal;
        case SDLK_KP_DIVIDE: return ImGuiKey_KeypadDivide;
        case SDLK_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
        case SDLK_KP_MINUS: return ImGuiKey_KeypadSubtract;
        case SDLK_KP_PLUS: return ImGuiKey_KeypadAdd;
        case SDLK_KP_ENTER: return ImGuiKey_KeypadEnter;
        case SDLK_KP_EQUALS: return ImGuiKey_KeypadEqual;
        case SDLK_LCTRL: return ImGuiKey_LeftCtrl;
        case SDLK_LSHIFT: return ImGuiKey_LeftShift;
        case SDLK_LALT: return ImGuiKey_LeftAlt;
        case SDLK_LGUI: return ImGuiKey_LeftSuper;
        case SDLK_RCTRL: return ImGuiKey_RightCtrl;
        case SDLK_RSHIFT: return ImGuiKey_RightShift;
        case SDLK_RALT: return ImGuiKey_RightAlt;
        case SDLK_RGUI: return ImGuiKey_RightSuper;
        case SDLK_APPLICATION: return ImGuiKey_Menu;
        case SDLK_0: return ImGuiKey_0;
        case SDLK_1: return ImGuiKey_1;
        case SDLK_2: return ImGuiKey_2;
        case SDLK_3: return ImGuiKey_3;
        case SDLK_4: return ImGuiKey_4;
        case SDLK_5: return ImGuiKey_5;
        case SDLK_6: return ImGuiKey_6;
        case SDLK_7: return ImGuiKey_7;
        case SDLK_8: return ImGuiKey_8;
        case SDLK_9: return ImGuiKey_9;
        case SDLK_a: return ImGuiKey_A;
        case SDLK_b: return ImGuiKey_B;
        case SDLK_c: return ImGuiKey_C;
        case SDLK_d: return ImGuiKey_D;
        case SDLK_e: return ImGuiKey_E;
        case SDLK_f: return ImGuiKey_F;
        case SDLK_g: return ImGuiKey_G;
        case SDLK_h: return ImGuiKey_H;
        case SDLK_i: return ImGuiKey_I;
        case SDLK_j: return ImGuiKey_J;
        case SDLK_k: return ImGuiKey_K;
        case SDLK_l: return ImGuiKey_L;
        case SDLK_m: return ImGuiKey_M;
        case SDLK_n: return ImGuiKey_N;
        case SDLK_o: return ImGuiKey_O;
        case SDLK_p: return ImGuiKey_P;
        case SDLK_q: return ImGuiKey_Q;
        case SDLK_r: return ImGuiKey_R;
        case SDLK_s: return ImGuiKey_S;
        case SDLK_t: return ImGuiKey_T;
        case SDLK_u: return ImGuiKey_U;
        case SDLK_v: return ImGuiKey_V;
        case SDLK_w: return ImGuiKey_W;
        case SDLK_x: return ImGuiKey_X;
        case SDLK_y: return ImGuiKey_Y;
        case SDLK_z: return ImGuiKey_Z;
        case SDLK_F1: return ImGuiKey_F1;
        case SDLK_F2: return ImGuiKey_F2;
        case SDLK_F3: return ImGuiKey_F3;
        case SDLK_F4: return ImGuiKey_F4;
        case SDLK_F5: return ImGuiKey_F5;
        case SDLK_F6: return ImGuiKey_F6;
        case SDLK_F7: return ImGuiKey_F7;
        case SDLK_F8: return ImGuiKey_F8;
        case SDLK_F9: return ImGuiKey_F9;
        case SDLK_F10: return ImGuiKey_F10;
        case SDLK_F11: return ImGuiKey_F11;
        case SDLK_F12: return ImGuiKey_F12;
        default: break;
        }
    return ImGuiKey_None;
}

char UI::ImGuiKey_to_Charecter(ImGuiKey imgui_key, bool shift)
{
    if (!shift) {
        switch (imgui_key)
            {
            case ImGuiKey_0: return '0';
            case ImGuiKey_1: return '1';
            case ImGuiKey_2: return '2';
            case ImGuiKey_3: return '3';
            case ImGuiKey_4: return '4';
            case ImGuiKey_5: return '5';
            case ImGuiKey_6: return '6';
            case ImGuiKey_7: return '7';
            case ImGuiKey_8: return '8';
            case ImGuiKey_9: return '9';
            case ImGuiKey_A: return 'a';
            case ImGuiKey_B: return 'b';
            case ImGuiKey_C: return 'c';
            case ImGuiKey_D: return 'd';
            case ImGuiKey_E: return 'e';
            case ImGuiKey_F: return 'f';
            case ImGuiKey_G: return 'g';
            case ImGuiKey_H: return 'h';
            case ImGuiKey_I: return 'i';
            case ImGuiKey_J: return 'j';
            case ImGuiKey_K: return 'k';
            case ImGuiKey_L: return 'l';
            case ImGuiKey_M: return 'm';
            case ImGuiKey_N: return 'n';
            case ImGuiKey_O: return 'o';
            case ImGuiKey_P: return 'p';
            case ImGuiKey_Q: return 'q';
            case ImGuiKey_R: return 'r';
            case ImGuiKey_S: return 's';
            case ImGuiKey_T: return 't';
            case ImGuiKey_U: return 'u';
            case ImGuiKey_V: return 'v';
            case ImGuiKey_W: return 'w';
            case ImGuiKey_X: return 'x';
            case ImGuiKey_Y: return 'y';
            case ImGuiKey_Z: return 'z';
            case ImGuiKey_Space: return 0x20;
            default: break;
            }
    }
    else {
        switch (imgui_key)
            {
            case ImGuiKey_A: return 'A';
            case ImGuiKey_B: return 'B';
            case ImGuiKey_C: return 'C';
            case ImGuiKey_D: return 'D';
            case ImGuiKey_E: return 'E';
            case ImGuiKey_F: return 'F';
            case ImGuiKey_G: return 'G';
            case ImGuiKey_H: return 'H';
            case ImGuiKey_I: return 'I';
            case ImGuiKey_J: return 'J';
            case ImGuiKey_K: return 'K';
            case ImGuiKey_L: return 'L';
            case ImGuiKey_M: return 'M';
            case ImGuiKey_N: return 'N';
            case ImGuiKey_O: return 'O';
            case ImGuiKey_P: return 'P';
            case ImGuiKey_Q: return 'Q';
            case ImGuiKey_R: return 'R';
            case ImGuiKey_S: return 'S';
            case ImGuiKey_T: return 'T';
            case ImGuiKey_U: return 'U';
            case ImGuiKey_V: return 'V';
            case ImGuiKey_W: return 'W';
            case ImGuiKey_X: return 'X';
            case ImGuiKey_Y: return 'Y';
            case ImGuiKey_Z: return 'Z';
            case ImGuiKey_Space: return 0x20;
            default: break;
            }
    }
    return '?';
}

static consteval float hueToRgb(float p, float q, float t) {
    if (t < 0.f) t += 1.f;
    if (t > 1.f) t -= 1.f;
    if (t < 1.f/6.f) return p + (q - p) * 6.f * t;
    if (t < 1.f/2.f) return q;
    if (t < 2.f/3.f) return p + (q - p) * (2.f/3.f - t) * 6.f;
    return p;
}

static consteval ImVec4 hslToRgb(const float h, const float s, const float l) {
    float r, g, b;
    
    if (s == 0.f) {
        r = g = b = l;
    } else {
        const float q = l < 0.5f ? l * (1.f + s) : l + s - l * s;
        const float p = 2.f * l - q;
        r = hueToRgb(p, q, h + 1.f/3.f);
        g = hueToRgb(p, q, h);
        b = hueToRgb(p, q, h - 1.f/3.f);
    }
    
    return ImVec4(r, g, b, 1.f);
}

void UI::setBessDarkColors(void) {
    ImGuiStyle &style = ImGui::GetStyle();
    ImVec4 *colors = style.Colors;
    
    constexpr float baseHue = 0.67f;
    constexpr float controlHue = 0.70f;
    
    // Primary background
    colors[ImGuiCol_WindowBg] =             hslToRgb(baseHue, 0.12f, 0.08f);
    colors[ImGuiCol_MenuBarBg] =            hslToRgb(baseHue, 0.11f, 0.13f);
    colors[ImGuiCol_PopupBg] =              hslToRgb(baseHue, 0.10f, 0.20f);
    
    // Headers
    colors[ImGuiCol_Header] =               hslToRgb(baseHue, 0.10f, 0.20f);
    colors[ImGuiCol_HeaderHovered] =        hslToRgb(baseHue, 0.14f, 0.35f);
    colors[ImGuiCol_HeaderActive] =         hslToRgb(baseHue, 0.16f, 0.30f);
    
    // Buttons
    colors[ImGuiCol_Button] =               hslToRgb(controlHue + .02f, 0.34f, 0.23f);
    colors[ImGuiCol_ButtonHovered] =        hslToRgb(controlHue + .03f, 0.34f, 0.35f);
    colors[ImGuiCol_ButtonActive] =         hslToRgb(controlHue + .03f, 0.37f, 0.42f);
    
    // Frame BG
    colors[ImGuiCol_FrameBg] =              hslToRgb(baseHue, 0.09f, 0.16f);
    colors[ImGuiCol_FrameBgHovered] =       hslToRgb(baseHue, 0.10f, 0.24f);
    colors[ImGuiCol_FrameBgActive] =        hslToRgb(baseHue, 0.09f, 0.27f);
    
    // Tabs
    colors[ImGuiCol_Tab] =                  hslToRgb(baseHue, 0.10f, 0.20f);
    colors[ImGuiCol_TabHovered] =           hslToRgb(baseHue, 0.17f, 0.42f);
    colors[ImGuiCol_TabActive] =            hslToRgb(baseHue, 0.20f, 0.31f);
    colors[ImGuiCol_TabUnfocused] =         hslToRgb(baseHue, 0.13f, 0.15f);
    colors[ImGuiCol_TabUnfocusedActive] =   hslToRgb(baseHue, 0.11f, 0.22f);
    
    // Title
    colors[ImGuiCol_TitleBg] =              hslToRgb(baseHue, 0.11f, 0.13f);
    colors[ImGuiCol_TitleBgActive] =        hslToRgb(baseHue, 0.14f, 0.17f);
    colors[ImGuiCol_TitleBgCollapsed] =     hslToRgb(baseHue, 0.09f, 0.11f);
    
    // Borders
    colors[ImGuiCol_Border] =               hslToRgb(baseHue, 0.11f, 0.22f);
    colors[ImGuiCol_BorderShadow] =         hslToRgb(0.00f, 0.00f, 0.00f);
    
    // Text
    colors[ImGuiCol_Text] =                 hslToRgb(baseHue, 0.33f, 0.92f);
    colors[ImGuiCol_TextDisabled] =         hslToRgb(baseHue, 0.05f, 0.52f);
    
    // Highlights
    colors[ImGuiCol_CheckMark] =            hslToRgb(controlHue, 1.00f, 0.60f);
    colors[ImGuiCol_SliderGrab] =           hslToRgb(controlHue, 1.00f, 0.60f);
    colors[ImGuiCol_SliderGrabActive] =     hslToRgb(controlHue - .02f, 1.00f, 0.65f);
    colors[ImGuiCol_ResizeGrip] =           hslToRgb(controlHue, 1.00f, 0.60f);
    colors[ImGuiCol_ResizeGripHovered] =    hslToRgb(controlHue - .02f, 1.00f, 0.65f);
    colors[ImGuiCol_ResizeGripActive] =     hslToRgb(controlHue - .05f, 1.00f, 0.70f);
    
    colors[ImGuiCol_DockingPreview] =       hslToRgb(controlHue, 0.50f, 0.30f);
    colors[ImGuiCol_PlotLines] =            hslToRgb(controlHue, 0.80f, 0.70f);
    
    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] =          hslToRgb(baseHue, 0.09f, 0.11f);
    colors[ImGuiCol_ScrollbarGrab] =        hslToRgb(baseHue, 0.07f, 0.32f);
    colors[ImGuiCol_ScrollbarGrabHovered] = hslToRgb(baseHue, 0.11f, 0.45f);
    colors[ImGuiCol_ScrollbarGrabActive] =  hslToRgb(baseHue, 0.10f, 0.50f);
    
    // Style tweaks
    float radius = 10.f;
    style.WindowRounding = radius;
    style.FrameRounding = radius;
    style.GrabRounding = radius;
    style.TabRounding = radius;
    style.PopupRounding = radius;
    style.ScrollbarRounding = 5.0f;
    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(6, 4);
    style.ItemSpacing = ImVec2(8, 6);
    style.PopupBorderSize = 0.f;
}



#include <filesystem>

#define BIT(x) (1 << x)

static std::pair<bool, uint32_t> DirectoryTreeViewRecursive(const std::filesystem::path& path, uint32_t* count, int* selection_mask)
{
    ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanFullWidth;
    
    bool any_node_clicked = false;
    uint32_t node_clicked = 0;
    
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        ImGuiTreeNodeFlags node_flags = base_flags;
        const bool is_selected = (*selection_mask & BIT(*count)) != 0;
        if (is_selected)
            node_flags |= ImGuiTreeNodeFlags_Selected;
        
        std::string name = entry.path().string();
        
        auto lastSlash = name.find_last_of("/\\");
        lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
        name = name.substr(lastSlash, name.size() - lastSlash);
        
        bool entryIsFile = !std::filesystem::is_directory(entry.path());
        if (entryIsFile)
            node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        
        bool node_open = ImGui::TreeNodeEx((void*)(intptr_t)(*count), node_flags, "%s", name.c_str());
        
        if (ImGui::IsItemClicked()) {
            node_clicked = *count;
            any_node_clicked = true;
        }
        
        (*count)--;
        
        if (!entryIsFile) {
            if (node_open) {
                
                auto clickState = DirectoryTreeViewRecursive(entry.path(), count, selection_mask);
                
                if (!any_node_clicked) {
                    any_node_clicked = clickState.first;
                    node_clicked = clickState.second;
                }
                
                ImGui::TreePop();
            }
            else {
                for (const auto& e : std::filesystem::recursive_directory_iterator(entry.path())) {
                    (void)e;
                    (*count)--;
                }
            }
        }
    }
    
    return { any_node_clicked, node_clicked };
}

void UI::treeAssetsWidget(void)
{
    std::string directoryPath = "../../../shaders/";
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
    
    ImGui::Begin("Assets");
    
    if (ImGui::CollapsingHeader("Shaders")) {
        uint32_t count = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directoryPath)) {
            (void)entry;
            count++;
        }
        
        static int selection_mask = 0;
        
        auto clickState = DirectoryTreeViewRecursive(directoryPath, &count, &selection_mask);
        
        if (clickState.first) {
            // Update selection state
            // (process outside of tree loop to avoid visual inconsistencies during the clicking frame)
            if (ImGui::GetIO().KeyCtrl)
                selection_mask ^= BIT(clickState.second);          // CTRL+click to toggle
            else //if (!(selection_mask & (1 << clickState.second))) // Depending on selection behavior you want, may want to preserve selection when clicking on item that is part of the selection
                selection_mask = BIT(clickState.second);           // Click to single-select
        }
    }
    ImGui::End();
    
    ImGui::PopStyleVar();
}
