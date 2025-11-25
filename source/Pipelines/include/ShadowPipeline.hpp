//
//  ShadowPipeline.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 12/11/25.
//

#ifndef ShadowPipeline_hpp
#define ShadowPipeline_hpp

#include "PipelineWrapper.hpp"
#include "Primitive.hpp"

class ShadowPipeline : public PipelineWrapper {
public:
		struct FrameData {
				Primitive::Map& primitives;
				std::array<VkDescriptorBufferInfo, SwapChain::MAX_FRAMES_IN_FLIGHT> uboDescriptors;
		};

		ShadowPipeline(const Device& device, VkRenderPass renderPass, std::string shader, FrameData frameData)
				: PipelineWrapper(device, renderPass, shader), m_FrameData(frameData) { _inheritedConstructor(); }
				
		void render(VkCommandBuffer commandBuffer, int frameIndex) override;
				
private:
		Dependencies createLayoutDependencies(void) override;
		void customizePipelineConfig(PipelineConfigInfo& config) override;

		FrameData m_FrameData;
		
		struct {
				std::unique_ptr<DescriptorSetLayout> layout;
				std::unique_ptr<DescriptorPool> pool;
		} m_Descriptor;
		
		VkDescriptorSet m_DescriptorSets[SwapChain::MAX_FRAMES_IN_FLIGHT];
};

#endif /* ShadowPipeline_hpp */
