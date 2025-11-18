//
//  SkyboxPipeline.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 13/11/25.
//

#include "SkyboxPipeline.hpp"


void SkyboxPipeline::customizePipelineConfig(PipelineConfigInfo& config) {
		config.renderPass = m_RenderPass;
		config.pipelineLayout = m_PipelineLayout;
		
		config.multisampleInfo.rasterizationSamples = m_SampleCount;
		config.multisampleInfo.sampleShadingEnable = VK_TRUE;
		config.multisampleInfo.minSampleShading = .2f;
		config.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
		config.rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		config.rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

SkyboxPipeline::Dependencies SkyboxPipeline::createLayoutDependencies(void) {
		m_Cube = std::make_unique<Model>(m_Device, Model::Data::makeSimpleCube(true));
		
		m_Descriptor.layout = DescriptorSetLayout::Builder(m_Device.device())
				.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
				.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.build_ptr();
		
		m_Descriptor.pool = DescriptorPool::Builder(m_Device.device())
				.setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
				.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT)
				.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
				.build_ptr();
		
		for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
				VkDescriptorSet descriptorSet;
				DescriptorWriter(*m_Descriptor.layout, *m_Descriptor.pool)
						.writeBuffer(0, &m_FrameData.uboDescriptors[i])
						.writeImage(1, m_FrameData.envImageDescriptor)
						.build(descriptorSet);
				
				m_DescriptorSets[i] = descriptorSet;
				
		}
		
		std::vector<VkDescriptorSetLayout> layouts(1);
		layouts[0] = *m_Descriptor.layout->getDescriptorSetLayout();
		
		return SkyboxPipeline::Dependencies {
				.descriptorSetLayouts = layouts,
				.pushConstantRanges = std::vector<VkPushConstantRange>(0)
		};
}

void SkyboxPipeline::render(VkCommandBuffer commandBuffer, int frameIndex) {
		if (getPipelineStatus() != Pipeline::Status::OK) {
				return;
		}
		
		m_Pipeline->bind(commandBuffer);
		
		vkCmdBindDescriptorSets(
				commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_PipelineLayout,
				0,
				1,
				&m_DescriptorSets[frameIndex],
				0,
				nullptr );
		
		m_Cube->bind(commandBuffer);
		m_Cube->draw(commandBuffer);
}
