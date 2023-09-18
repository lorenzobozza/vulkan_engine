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

struct PushConstantData {
    float exposure{};
    float peak_brightness{};
    float gamma{};
};

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
    VkPushConstantRange pushConstantRange;

    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstantData);
    
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{compositionSetLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &compositionSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
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
      shaderPath+".vert.spv",
      shaderPath+".frag.spv",
      pipelineConfig);
      
    Model::Data data;
    data.vertices = {
        {{-1.f, -1.f, .0f}, {}, {}, {}, {0.f, 0.f}},
        {{1.f, -1.f, .0f}, {}, {}, {}, {1.f, 0.f}},
        {{1.f, 1.f, .0f}, {}, {}, {}, {1.f, 1.f}},
        {{-1.f, 1.f, .0f}, {}, {}, {}, {0.f, 1.f}}
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
    
    PushConstantData push{};
    push.exposure = exposure;
    push.peak_brightness = peak_brightness;
    push.gamma = gamma;
    
    vkCmdPushConstants(
        commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(PushConstantData),
        &push);
    
    quad->bind(commandBuffer);
    quad->draw(commandBuffer);
}
