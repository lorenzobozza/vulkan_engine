//
//  ScenePipeline.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 12/11/25.
//

#include "ShadowPipeline.hpp"

struct PushConstantData {
    glm::mat4 modelMatrix{1.f};
};

void ShadowPipeline::customizePipelineConfig(PipelineConfigInfo& config) {
    config.renderPass = m_RenderPass;
    config.pipelineLayout = m_PipelineLayout;
    
    config.multisampleInfo.rasterizationSamples = m_SampleCount;
    config.multisampleInfo.sampleShadingEnable = VK_TRUE;
    config.multisampleInfo.minSampleShading = .2f;
    
    config.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    config.rasterizationInfo.cullMode = VK_CULL_MODE_FRONT_BIT; // Front culling for shadow map
    config.rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

ShadowPipeline::Dependencies ShadowPipeline::createLayoutDependencies(void) {
    
    m_Descriptor.layout = DescriptorSetLayout::Builder(m_Device)
        .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
        .build_ptr();
    
    m_Descriptor.pool = DescriptorPool::Builder(m_Device)
        .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT)
        .build_ptr();
    
    
    for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorSet descriptorSet;
        DescriptorWriter(*m_Descriptor.layout, *m_Descriptor.pool)
            .writeBuffer(0, &m_FrameData.uboDescriptors[i])
            .build(descriptorSet);
        
        m_DescriptorSets[i] = descriptorSet;
    }
    
    std::vector<VkDescriptorSetLayout> layouts(1);
    layouts[0] = *m_Descriptor.layout->getDescriptorSetLayout();
    
    std::vector<VkPushConstantRange> push(1);
    push[0] = VkPushConstantRange {
        .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
        .offset = 0,
        .size = sizeof(PushConstantData)
    };
    
    return ShadowPipeline::Dependencies {
        .descriptorSetLayouts = layouts,
        .pushConstantRanges = push
    };
}

void ShadowPipeline::render(VkCommandBuffer commandBuffer, int frameIndex) {
    m_Pipeline->bind(commandBuffer);
    
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_PipelineLayout,
                            0,
                            1,
                            &m_DescriptorSets[frameIndex],
                            0,
                            nullptr);
    
    for (auto &kv : m_FrameData.primitives) {
        auto &primitive = kv.second;
        
        PushConstantData push{};
        push.modelMatrix = primitive.transform.mat4();
        
        vkCmdPushConstants(commandBuffer,
                           m_PipelineLayout,
                           VK_SHADER_STAGE_ALL_GRAPHICS,
                           0,
                           sizeof(PushConstantData),
                           &push);
        
        primitive.model->bind(commandBuffer);
        primitive.model->draw(commandBuffer);
    }
    
}
