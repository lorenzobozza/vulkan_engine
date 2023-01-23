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
  int textureIndex{};
  float metalness{};
  float roughness{};
  alignas(16) glm::vec3 color{};
};

RenderSystem::RenderSystem(
    Device& passDevice,
    VkRenderPass renderPass,
    VkDescriptorSetLayout globalSetLayout,
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

void RenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
  VkPushConstantRange pushConstantRanges[1];
  
  pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
  pushConstantRanges[0].offset = 0;
  pushConstantRanges[0].size = sizeof(PushConstantData);
  
  //pushConstantRanges[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  //pushConstantRanges[1].offset = sizeof(PushConstantData); // offset by previus push_constant size
  //pushConstantRanges[1].size = sizeof(PushCostant2);
  
  std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = 1;
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
    pipelineConfig.multisampleInfo.rasterizationSamples = sampleCount;
    pipelineConfig.multisampleInfo.sampleShadingEnable = VK_TRUE;
    pipelineConfig.multisampleInfo.minSampleShading = .2f;

    pipelineConfig.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    pipelineConfig.rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    pipelineConfig.rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    pipeline = std::make_unique<Pipeline>(
        device,
        shaderPath+".vert.spv",
        shaderPath+".frag.spv",
        pipelineConfig);
    }

void RenderSystem::renderSolidObjects(FrameInfo &frameInfo) {
  pipeline->bind(frameInfo.commandBuffer);

  for (auto &kv : frameInfo.solidObjects) {
    auto &obj = kv.second;
    
    vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0,
        1,
        &frameInfo.globalDescriptorSet[obj.textureIndex],
        0,
        nullptr
    );
    
    PushConstantData push{};
    push.modelMatrix = obj.transform.mat4();
    push.textureIndex = obj.textureIndex;
    push.metalness = obj.metalness;
    push.roughness = obj.roughness;
    push.color = obj.color;

    vkCmdPushConstants(
        frameInfo.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_ALL_GRAPHICS,
        0,
        sizeof(PushConstantData),
        &push);
        
    /*
    vkCmdPushConstants(
        frameInfo.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        sizeof(PushConstantData), // offset by previus push_constant size
        sizeof(PushConstant2),
        &push2);
    */
        
    obj.model->bind(frameInfo.commandBuffer);
    obj.model->draw(frameInfo.commandBuffer);
  }
}
