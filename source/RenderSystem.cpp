//
//  RendererSystem.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 09/11/21.
//

#include "include/RenderSystem.hpp"


//std
#include <array>
#include <cassert>
#include <stdexcept>
#include <iostream>

struct PushConstantData {
  glm::mat4 modelMatrix{1.f};
  int textureIndex{};
  float metalness{};
  float roughness{};
  alignas(16) glm::vec4 color{};
  int alphaMode{};
  float alphaCutoff{};
  int backFace{-1};
};

RenderSystem::RenderSystem(
    Device& passDevice,
    VkRenderPass renderPass,
    std::string dynamicShaderPath,
    const VkDescriptorSetLayout* globalSetLayout,
    const unsigned int setLayoutCount,
    VkSampleCountFlagBits samples) : device{passDevice}, shaderPath{dynamicShaderPath}, sampleCount{samples} {
  createPipelineLayout(globalSetLayout, setLayoutCount);
  createPipeline(renderPass);
}

RenderSystem::~RenderSystem() {
  vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
}

void RenderSystem::recreatePipeline(VkRenderPass renderPass, VkSampleCountFlagBits samples) {
    pipeline.reset();
    sampleCount = samples;
    createPipeline(renderPass);
}

void RenderSystem::createPipelineLayout(const VkDescriptorSetLayout* globalSetLayout, const unsigned int setLayoutCount) {

  VkPushConstantRange pushConstantRange {
    .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
    .offset = 0,
    .size = sizeof(PushConstantData)
  };
  
  VkPipelineLayoutCreateInfo pipelineLayoutInfo {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .setLayoutCount = setLayoutCount,
    .pSetLayouts = globalSetLayout,
    .pushConstantRangeCount = 1,
    .pPushConstantRanges = &pushConstantRange
  };
  
  if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create pipeline layout!");
  }
  
}

void RenderSystem::createPipeline(VkRenderPass renderPass) {
  assert(pipelineLayout != VK_NULL_HANDLE && "Cannot create pipeline before pipeline layout");

    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;
    pipelineConfig.multisampleInfo.rasterizationSamples = sampleCount;
    pipelineConfig.multisampleInfo.sampleShadingEnable = VK_TRUE;
    pipelineConfig.multisampleInfo.minSampleShading = .2f;

    pipelineConfig.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cull = (shaderPath == "depth") ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT;
    pipelineConfig.rasterizationInfo.cullMode = cull;
    pipelineConfig.rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    pipeline = std::make_unique<Pipeline>(
        device,
        shaderPath+".vert",
        shaderPath+".frag",
        pipelineConfig);
}

void RenderSystem::renderSolidObjects(FrameInfo &frameInfo) {
  if (getPipelineStatus() != Pipeline::Status::OK) {
    return;
  }
  
  pipeline->bind(frameInfo.commandBuffer);

    vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0,
        1,
        &frameInfo.mainDescriptorSet,
        0,
        nullptr
    );

  for (auto &kv : frameInfo.primitives) {
    auto &obj = kv.second;
    
    if (!frameInfo.materialDescriptorSets.empty()) {
        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            1,
            1,
            &frameInfo.materialDescriptorSets[obj.material],
            0,
            nullptr
        );
    }
    
    PushConstantData push{};
    push.modelMatrix = obj.transform.mat4();
    push.textureIndex = frameInfo.materials[obj.material].getTextureBitmap();
    push.metalness = frameInfo.materials[obj.material].metalness;
    push.roughness = frameInfo.materials[obj.material].roughness;
    push.color = frameInfo.materials[obj.material].color;
    push.alphaMode = frameInfo.materials[obj.material].alphaMode;
    push.alphaCutoff = frameInfo.materials[obj.material].alphaCutoff;

    vkCmdPushConstants(
        frameInfo.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_ALL_GRAPHICS,
        0,
        sizeof(PushConstantData),
        &push);
        
    obj.model->bind(frameInfo.commandBuffer);
    obj.model->draw(frameInfo.commandBuffer);
  }
}

void RenderSystem::renderSolidObjects(FrameInfoNoMaterials &frameInfo) {
  if (getPipelineStatus() != Pipeline::Status::OK) {
    return;
  }

  pipeline->bind(frameInfo.commandBuffer);
  
      vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0,
        1,
        &frameInfo.mainDescriptorSet,
        0,
        nullptr
    );

  for (auto &kv : frameInfo.primitives) {
    auto &obj = kv.second;
    
    PushConstantData push{};
    push.modelMatrix = obj.transform.mat4();

    vkCmdPushConstants(
        frameInfo.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_ALL_GRAPHICS,
        0,
        sizeof(PushConstantData),
        &push);
        
    obj.model->bind(frameInfo.commandBuffer);
    obj.model->draw(frameInfo.commandBuffer);
  }
}
