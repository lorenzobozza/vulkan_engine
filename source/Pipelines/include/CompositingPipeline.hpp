//
//  CompositingPipeline.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 27/11/22.
//

#ifndef CompositingPipeline_hpp
#define CompositingPipeline_hpp

#include "PipelineWrapper.hpp"

class CompositingPipeline : public PipelineWrapper {
public:
    CompositingPipeline(Device& device, VkRenderPass renderPass, std::string shader, std::vector<VkDescriptorSet>* frameDescriptorSets)
    : PipelineWrapper(device, renderPass, shader), m_DescriptorSets(frameDescriptorSets) { _inheritedConstructor(); }
    
    
    void render(VkCommandBuffer commandBuffer, int frameIndex) override;
    
    float exposure = 1.5f;
    float peak_brightness = 2.f;
    float gamma = 2.2f;
    unsigned int debugMode = 0;
    
private:
    Dependencies createLayoutDependencies(void) override;
    void customizePipelineConfig(PipelineConfigInfo& config) override;
    
    std::unique_ptr<Model> m_Quad;
    
    struct {
        std::unique_ptr<DescriptorSetLayout> layout;
        std::unique_ptr<DescriptorPool> pool;
    } m_Descriptor;
    
    std::vector<VkDescriptorSet>* m_DescriptorSets;
};

#endif /* CompositingPipeline_hpp */
