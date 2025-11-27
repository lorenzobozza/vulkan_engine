//
//  SkyboxPipeline.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 13/11/25.
//

#ifndef SkyboxPipeline_hpp
#define SkyboxPipeline_hpp

#include "PipelineWrapper.hpp"
#include "Primitive.hpp"

class SkyboxPipeline : public PipelineWrapper {
public:
		struct FrameData {
				std::array<VkDescriptorBufferInfo, SwapChain::MAX_FRAMES_IN_FLIGHT> uboDescriptors;
				VkDescriptorImageInfo* envImageDescriptor;
		};

		SkyboxPipeline(const Device& device, VkRenderPass renderPass, std::string shader, FrameData frameData)
				: PipelineWrapper(device, renderPass, shader), m_FrameData(frameData) { _inheritedConstructor(); }
				
		void render(VkCommandBuffer commandBuffer, int frameIndex) override;
				
private:
		Dependencies createLayoutDependencies(void) override;
		void customizePipelineConfig(PipelineConfigInfo& config) override;

		FrameData m_FrameData;
		std::unique_ptr<Mesh> m_Cube;
				
		struct {
				std::unique_ptr<DescriptorSetLayout> layout;
				std::unique_ptr<DescriptorPool> pool;
		} m_Descriptor;
		
		VkDescriptorSet m_DescriptorSets[SwapChain::MAX_FRAMES_IN_FLIGHT];
};


#endif /* SkyboxPipeline_hpp */
