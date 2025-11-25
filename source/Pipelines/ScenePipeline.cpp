//
//  ScenePipeline.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 12/11/25.
//

#include "ScenePipeline.hpp"

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

void ScenePipeline::customizePipelineConfig(PipelineConfigInfo& config) {
		config.renderPass = m_RenderPass;
		config.pipelineLayout = m_PipelineLayout;
		
		config.multisampleInfo.rasterizationSamples = m_SampleCount;
		config.multisampleInfo.sampleShadingEnable = VK_TRUE;
		config.multisampleInfo.minSampleShading = .2f;
		config.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
		config.rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		config.rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

ScenePipeline::Dependencies ScenePipeline::createLayoutDependencies(void) {
		
		m_MainDescriptor.layout = DescriptorSetLayout::Builder(m_Device)
				.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
				.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.build_ptr();
		
		m_MaterialDescriptor.layout = DescriptorSetLayout::Builder(m_Device)
				.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
				.build_ptr();
		
		const uint32_t numOfMaterials = (uint32_t)m_FrameData.materials.size();
		
		m_MainDescriptor.pool = DescriptorPool::Builder(m_Device)
				.setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
				.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT)
				.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
				.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
				.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
				.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT)
				.build_ptr();
		
		m_MaterialDescriptor.pool = DescriptorPool::Builder(m_Device)
				.setMaxSets(numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
				.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
				.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
				.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
				.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numOfMaterials * SwapChain::MAX_FRAMES_IN_FLIGHT)
				.build_ptr();
		
		for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
				VkDescriptorSet descriptorSet;
				DescriptorWriter(*m_MainDescriptor.layout, *m_MainDescriptor.pool)
						.writeBuffer(0, &m_FrameData.uboDescriptors[i])
						.writeImage(1, m_FrameData.imageDescriptors.irradiance)
						.writeImage(2, m_FrameData.imageDescriptors.reflection)
						.writeImage(3, m_FrameData.imageDescriptors.brdf)
						.writeImage(4, &m_FrameData.imageDescriptors.shadow[i])
						.build(descriptorSet);
				
				m_MainDescriptorSets[i] = descriptorSet;
				
				for (auto& kv : m_FrameData.materials) {
						auto& name = kv.first;
						auto& material = kv.second;
						VkDescriptorImageInfo color = material.getColorTextureIF(),
						normal = material.getNormalTextureIF(),
						occlusion = material.getOcclusionTextureIF(),
						metalRough = material.getMetalRoughTextureIF();
						
						DescriptorWriter(*m_MaterialDescriptor.layout, *m_MaterialDescriptor.pool)
								.writeImage(0, &color)				// Color
								.writeImage(1, &normal)				// Normal
								.writeImage(2, &metalRough)		// Metallic-Roughness
								.writeImage(3, &occlusion)		// Occlusion
								.build(descriptorSet);
						
						m_MaterialDescriptorSets[i].emplace(name, descriptorSet);
				}
		}
		
		std::vector<VkDescriptorSetLayout> layouts(2);
		layouts[0] = *m_MainDescriptor.layout->getDescriptorSetLayout();
		layouts[1] = *m_MaterialDescriptor.layout->getDescriptorSetLayout();
		
		std::vector<VkPushConstantRange> push(1);
		push[0] = VkPushConstantRange {
				.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
				.offset = 0,
				.size = sizeof(PushConstantData)
		};
		
		return ScenePipeline::Dependencies {
				.descriptorSetLayouts = layouts,
				.pushConstantRanges = push
		};
}

void ScenePipeline::render(VkCommandBuffer commandBuffer, int frameIndex) {
		m_Pipeline->bind(commandBuffer);
		
		vkCmdBindDescriptorSets(
				commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_PipelineLayout,
				0,
				1,
				&m_MainDescriptorSets[frameIndex],
				0,
				nullptr );
		
		for (auto &kv : m_FrameData.primitives) {
				auto &primitive = kv.second;

				vkCmdBindDescriptorSets(
						commandBuffer,
						VK_PIPELINE_BIND_POINT_GRAPHICS,
						m_PipelineLayout,
						1,
						1,
						&m_MaterialDescriptorSets[frameIndex][primitive.material],
						0,
						nullptr );
				
				PushConstantData push{};
				push.modelMatrix = primitive.transform.mat4();
				push.textureIndex = m_FrameData.materials[primitive.material].getTextureBitmap();
				push.metalness = m_FrameData.materials[primitive.material].metalness;
				push.roughness = m_FrameData.materials[primitive.material].roughness;
				push.color = m_FrameData.materials[primitive.material].color;
				push.alphaMode = m_FrameData.materials[primitive.material].alphaMode;
				push.alphaCutoff = m_FrameData.materials[primitive.material].alphaCutoff;
				
				vkCmdPushConstants(
					 commandBuffer,
					 m_PipelineLayout,
					 VK_SHADER_STAGE_ALL_GRAPHICS,
					 0,
					 sizeof(PushConstantData),
					 &push);
				
				primitive.model->bind(commandBuffer);
				primitive.model->draw(commandBuffer);
		}
		
}
