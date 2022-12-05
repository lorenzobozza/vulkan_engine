//
//  CompositionPipeline.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 27/11/22.
//

#include "include/CompositionPipeline.hpp"

//std
#include <array>
#include <cassert>
#include <stdexcept>

CompositionPipeline::CompositionPipeline(
    Device& passDevice,
    VkRenderPass renderPass,
    VkDescriptorSetLayout compositionSetLayout,
    std::string dynamicShaderPath) : device{passDevice}, shaderPath{dynamicShaderPath} {
  createPipelineLayout(compositionSetLayout);
  createPipeline(renderPass);
}

CompositionPipeline::~CompositionPipeline() {
  vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
}

void CompositionPipeline::createPipelineLayout(VkDescriptorSetLayout compositionSetLayout) {
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{compositionSetLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &compositionSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }
}

void CompositionPipeline::createPipeline(VkRenderPass renderPass) {
    assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;
    pipelineConfig.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipelineConfig.multisampleInfo.sampleShadingEnable = VK_TRUE;
    pipelineConfig.multisampleInfo.minSampleShading = .2f;
    pipeline = std::make_unique<Pipeline>(
      device,
      "composition.vert.spv",
      "composition.frag.spv",
      pipelineConfig);
      
    Model::Data data;
    data.vertices = {
        {{-1.f, -1.f, .0f}, {}, {}, {0.f, 0.f}},
        {{1.f, -1.f, .0f}, {}, {}, {1.f, 0.f}},
        {{1.f, 1.f, .0f}, {}, {}, {1.f, 1.f}},
        {{-1.f, 1.f, .0f}, {}, {}, {0.f, 1.f}}
    };
    data.indices = {
        0,1,2,2,3,0
    };
    
    quad = std::make_unique<Model>(device, data);
}

void CompositionPipeline::renderSceneToSwapChain(VkCommandBuffer commandBuffer, VkDescriptorSet &descriptorSets) {
    pipeline->bind(commandBuffer);
              
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0,
        1,
        &descriptorSets,
        0,
        nullptr
    );
    
    quad->bind(commandBuffer);
    quad->draw(commandBuffer);
}
