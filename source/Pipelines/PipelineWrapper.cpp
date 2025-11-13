//
//  RendererSystem.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 09/11/21.
//

#include "PipelineWrapper.hpp"

void PipelineWrapper::_inheritedConstructor(void) {
		createPipelineLayout();
		createPipeline();
}

PipelineWrapper::~PipelineWrapper() {
		vkDestroyPipelineLayout(m_Device.device(), m_PipelineLayout, nullptr);
}

void PipelineWrapper::createPipelineLayout(void) {
  Dependencies deps = createLayoutDependencies();
  
  VkPipelineLayoutCreateInfo pipelineLayoutInfo {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .setLayoutCount = static_cast<uint32_t>(deps.descriptorSetLayouts.size()),
    .pSetLayouts = deps.descriptorSetLayouts.data(),
    .pushConstantRangeCount = static_cast<uint32_t>(deps.pushConstantRanges.size()),
    .pPushConstantRanges = deps.pushConstantRanges.data()
  };
  
  if (vkCreatePipelineLayout(m_Device.device(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
		// TODO: Handle in a non-critical way (implies making this func public)
    throw std::runtime_error("Failed to create pipeline layout!");
  }
  
}

void PipelineWrapper::createPipeline(void) {
		// TODO: Handle in a non-critical way (we already check pipeline status)
		assert(m_PipelineLayout != VK_NULL_HANDLE && "Cannot create pipeline before pipeline layout");
		
		PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		customizePipelineConfig(pipelineConfig);
		
		if (str_frag == "NULL") str_frag = str_vert;
		m_Pipeline = std::make_unique<Pipeline>(
																						m_Device,
																						str_vert + ".vert",
																						str_frag + ".frag",
																						pipelineConfig);
}

void PipelineWrapper::destroyPipeline(void) {
		m_Pipeline.reset();
}

void PipelineWrapper::recreatePipeline(VkRenderPass renderPass, VkSampleCountFlagBits samples) {
		if (renderPass != VK_NULL_HANDLE) {
				m_RenderPass = renderPass;
		}
		if (samples != VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM) {
				m_SampleCount = samples;
		}
		
		destroyPipeline();
		createPipeline();
}




// Example
void PipelineWrapper::customizePipelineConfig(PipelineConfigInfo& config) {
    config.renderPass = m_RenderPass;
    config.pipelineLayout = m_PipelineLayout;
    
    config.multisampleInfo.rasterizationSamples = m_SampleCount;
    config.multisampleInfo.sampleShadingEnable = VK_TRUE;
    config.multisampleInfo.minSampleShading = .2f;
    config.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    config.rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    config.rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
}
// Example
PipelineWrapper::Dependencies PipelineWrapper::createLayoutDependencies(void) {
		std::vector<VkPushConstantRange> push(1);
		push[0] = VkPushConstantRange {
				.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
				.offset = 0,
				.size = sizeof(int)
		};
		
		return Dependencies {
				.descriptorSetLayouts = std::vector<VkDescriptorSetLayout>(),
				.pushConstantRanges = push
		};
}
// Example
void PipelineWrapper::render(VkCommandBuffer commandBuffer, int frameIndex) {
  
}
