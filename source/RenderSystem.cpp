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
    const VkDescriptorSetLayout* globalSetLayout,
    std::string dynamicShaderPath,
    VkSampleCountFlagBits samples) : device{passDevice}, shaderPath{dynamicShaderPath}, sampleCount{samples} {
  createPipelineLayout(globalSetLayout);
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

void RenderSystem::createPipelineLayout(const VkDescriptorSetLayout* globalSetLayout) {
  VkPushConstantRange pushConstantRanges[1];
  
  pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
  pushConstantRanges[0].offset = 0;
  pushConstantRanges[0].size = sizeof(PushConstantData);
  
  //pushConstantRanges[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  //pushConstantRanges[1].offset = sizeof(PushConstantData); // offset by previus push_constant size
  //pushConstantRanges[1].size = sizeof(PushCostant2);
  
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = globalSetLayout;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges;
  if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
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
    pipelineConfig.rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
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

  for (auto &kv : frameInfo.primitives) {
    auto &obj = kv.second;
    
    vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0,
        1,
        &frameInfo.descriptorSet[obj.material],
        0,
        nullptr
    );
    
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

  for (auto &kv : frameInfo.primitives) {
    auto &obj = kv.second;
    
    vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0,
        1,
        &frameInfo.descriptorSet,
        0,
        nullptr
    );
    
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
