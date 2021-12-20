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

struct PushConstantData {
  glm::mat4 modelMatrix{1.f};
  glm::mat4 normalMatrix{1.f};
};

struct PushObjInfo {
    unsigned int textureIndex{0};
};

RenderSystem::RenderSystem(
    Device& passDevice,
    VkRenderPass renderPass,
    VkDescriptorSetLayout globalSetLayout,
    std::string dynamicShaderPath) : device{passDevice}, shaderPath{dynamicShaderPath} {
  createPipelineLayout(globalSetLayout);
  createPipeline(renderPass);
}

RenderSystem::~RenderSystem() {
  vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
}

void RenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
  VkPushConstantRange pushConstantRanges[2];
  
  pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  pushConstantRanges[0].offset = 0;
  pushConstantRanges[0].size = sizeof(PushConstantData);
  
  pushConstantRanges[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRanges[1].offset = sizeof(PushConstantData); // offset by previus push_constant size
  pushConstantRanges[1].size = sizeof(PushObjInfo);
  
  std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = 2;
  pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges;
  if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }
}

void RenderSystem::createPipeline(VkRenderPass renderPass) {
  assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

  PipelineConfigInfo pipelineConfig{};
  Pipeline::defaultPipelineConfigInfo(pipelineConfig);
  pipelineConfig.renderPass = renderPass;
  pipelineConfig.pipelineLayout = pipelineLayout;
  pipelineConfig.multisampleInfo.rasterizationSamples = device.msaaSamples;
  pipeline = std::make_unique<Pipeline>(
      device,
      shaderPath+".vert.spv",
      shaderPath+".frag.spv",
      pipelineConfig);
}

void RenderSystem::renderSolidObjects(FrameInfo &frameInfo) {
  pipeline->bind(frameInfo.commandBuffer);
  
    vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0,
        1,
        &frameInfo.globalDescriptorSet,
        0,
        nullptr
    );

  for (auto &kv : frameInfo.solidObjects) {
    auto &obj = kv.second;
  
    PushConstantData push{};
    push.modelMatrix = obj.transform.mat4();
    push.normalMatrix = obj.transform.normalMatrix();
    
    PushObjInfo pushInfo{};
    pushInfo.textureIndex = obj.textureIndex;

    vkCmdPushConstants(
        frameInfo.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(PushConstantData),
        &push);
        
    vkCmdPushConstants(
        frameInfo.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        sizeof(PushConstantData), // offset by previus push_constant size
        sizeof(PushObjInfo),
        &pushInfo);
        
    obj.model->bind(frameInfo.commandBuffer);
    obj.model->draw(frameInfo.commandBuffer);
  }
}
