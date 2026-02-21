//
//  CompositionPipeline.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 27/11/22.
//

#include "CompositingPipeline.hpp"
#include "Log.hpp"

struct PushConstantData {
    float exposure{};
    float peak_brightness{};
    float gamma{};
    unsigned int debugMode{};
};

void CompositingPipeline::customizePipelineConfig(PipelineConfigInfo& config) {
    config.renderPass = m_RenderPass;
    config.pipelineLayout = m_PipelineLayout;
    
    config.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    config.multisampleInfo.sampleShadingEnable = VK_FALSE;
    config.multisampleInfo.minSampleShading = .2f;
}

CompositingPipeline::Dependencies CompositingPipeline::createLayoutDependencies(void) {
    Mesh::Data data;
    data.vertices = {
        {{-1.f, -1.f, .0f}, {}, {}, {}, {0.f, 0.f}},
        {{1.f, -1.f, .0f}, {}, {}, {}, {1.f, 0.f}},
        {{1.f, 1.f, .0f}, {}, {}, {}, {1.f, 1.f}},
        {{-1.f, 1.f, .0f}, {}, {}, {}, {0.f, 1.f}}
    };
    data.indices = {
        0,1,2,2,3,0
    };
    m_Quad = std::make_unique<Mesh>(m_Device, data);
    
    m_Descriptor.layout = DescriptorSetLayout::Builder(m_Device)
        .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build_ptr();
    
    m_Descriptor.pool = DescriptorPool::Builder(m_Device)
        .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
        .build_ptr();
    
    
    std::vector<VkDescriptorSetLayout> layouts(1);
    layouts[0] = *m_Descriptor.layout->getDescriptorSetLayout();
    
    std::vector<VkPushConstantRange> push(1);
    push[0] = VkPushConstantRange {
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(PushConstantData)
    };
    
    return CompositingPipeline::Dependencies {
        .descriptorSetLayouts = layouts,
        .pushConstantRanges = push
    };
}

void CompositingPipeline::render(VkCommandBuffer commandBuffer, int frameIndex) {
    m_Pipeline->bind(commandBuffer);
    
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_PipelineLayout,
                            0,
                            1,
                            &(m_DescriptorSets->at(frameIndex)),
                            0,
                            nullptr);
    
    PushConstantData push{};
    push.exposure = exposure;
    push.peak_brightness = peak_brightness;
    push.gamma = gamma;
    push.debugMode = debugMode;
    
    vkCmdPushConstants(commandBuffer,
                       m_PipelineLayout,
                       VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(PushConstantData),
                       &push);
    
    m_Quad->bind(commandBuffer);
    m_Quad->draw(commandBuffer);
}
