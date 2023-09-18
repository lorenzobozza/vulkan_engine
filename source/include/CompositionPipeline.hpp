//
//  CompositionPipeline.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 27/11/22.
//

#ifndef CompositionPipeline_hpp
#define CompositionPipeline_hpp

#include "Device.hpp"
#include "SwapChain.hpp"
#include "Pipeline.hpp"

//std
#include <memory>
#include <vector>
#include <string>


class CompositionPipeline {
 public:
  CompositionPipeline(
    Device &passDevice,
    VkRenderPass renderPass,
    VkDescriptorSetLayout compositionSetLayout,
    std::string dynamicShaderPath);
  ~CompositionPipeline();

  CompositionPipeline(const CompositionPipeline &) = delete;
  CompositionPipeline &operator=(const CompositionPipeline &) = delete;

  virtual void renderSceneToSwapChain(VkCommandBuffer commandBuffer, VkDescriptorSet &descriptorSets);
  
  float exposure = 1.5f;
  float peak_brightness = 2.f;
  float gamma = 2.2f;

 private:
  void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
  void createPipeline(VkRenderPass renderPass);

protected:
    Device &device;

    std::unique_ptr<Model> quad;
    std::unique_ptr<Pipeline> pipeline;
    VkPipelineLayout pipelineLayout;

    std::string shaderPath;
};

#endif /* CompositionPipeline_hpp */
