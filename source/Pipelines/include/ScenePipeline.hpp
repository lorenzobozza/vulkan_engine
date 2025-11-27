//
//  ScenePipeline.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 12/11/25.
//

#ifndef ScenePipeline_hpp
#define ScenePipeline_hpp

#include "PipelineWrapper.hpp"
#include "Primitive.hpp"
#include "Material.hpp"

class ScenePipeline : public PipelineWrapper {
public:
		static constexpr int MAX_LIGHTS = 8;
		struct UniformBuffer {
				glm::mat4 projectionView{1.f};
				glm::mat4 viewMatrix{1.f};
				glm::mat4 invViewMatrix{1.f};
				
				glm::mat4 lightSpaceMatrix{1.f};
				glm::vec4 lightVector[MAX_LIGHTS]{};
				glm::vec4 lightChroma[MAX_LIGHTS]{};
				unsigned int lightInfo;
				
				unsigned int debugMode{0};
		};
		struct FrameData {
				Primitive::Map& primitives;
				Assets& assets;
				std::array<VkDescriptorBufferInfo, SwapChain::MAX_FRAMES_IN_FLIGHT> uboDescriptors;
				struct {
						VkDescriptorImageInfo *brdf, *reflection, *irradiance, *shadow;
				} imageDescriptors;
		};

		ScenePipeline(const Device& device, VkRenderPass renderPass, std::string shader, FrameData frameData)
				: PipelineWrapper(device, renderPass, shader), m_FrameData(frameData) { _inheritedConstructor(); }
				
		void render(VkCommandBuffer commandBuffer, int frameIndex) override;
				
private:
		Dependencies createLayoutDependencies(void) override;
		void customizePipelineConfig(PipelineConfigInfo& config) override;

		FrameData m_FrameData;
		
		struct {
				std::unique_ptr<DescriptorSetLayout> layout;
				std::unique_ptr<DescriptorPool> pool;
		} m_MainDescriptor, m_MaterialDescriptor;
		
		VkDescriptorSet m_MainDescriptorSets[SwapChain::MAX_FRAMES_IN_FLIGHT];
    std::unordered_map<std::string, VkDescriptorSet> m_MaterialDescriptorSets[SwapChain::MAX_FRAMES_IN_FLIGHT];
};

#endif /* ScenePipeline_hpp */
