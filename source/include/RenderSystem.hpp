//
//  RenderSystem.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 09/11/21.
//

#ifndef RenderSystem_hpp
#define RenderSystem_hpp

#include "Device.hpp"
#include "Pipeline.hpp"
#include "SolidObject.hpp"
#include "Camera.hpp"
#include "FrameInfo.hpp"

//std
#include <memory>
#include <vector>
#include <string>


class RenderSystem {
 public:
  RenderSystem(
    Device &passDevice,
    VkRenderPass renderPass,
    VkDescriptorSetLayout globalSetLayout,
    std::string dynamicShaderPath,
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
  ~RenderSystem();

  RenderSystem(const RenderSystem &) = delete;
  RenderSystem &operator=(const RenderSystem &) = delete;

  virtual void renderSolidObjects(FrameInfo &frameInfo);

 private:
  void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
  void createPipeline(VkRenderPass renderPass);

protected:
    Device &device;

    std::unique_ptr<Pipeline> pipeline;
    VkPipelineLayout pipelineLayout;
    VkSampleCountFlagBits samples;
    std::string shaderPath;
};

#endif /* RenderSystem_hpp */
