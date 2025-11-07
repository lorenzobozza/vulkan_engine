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
#include "Primitive.hpp"
#include "Camera.hpp"
#include "FrameInfo.hpp"
#include "Material.hpp"

//std
#include <memory>
#include <vector>
#include <string>

struct GlobalUbo {
    glm::mat4 projectionView{1.f};
    glm::vec4 ambientLightColor{1.f, 1.f, 1.f, .1f};
    glm::vec4 lightPosition[2] = {{.0f,-1.f,.0f,.0f},{.0f,-1.f,.0f,.0f}};
    glm::vec4 lightColor{1.f, 1.f, 1.f, 10.f};
    glm::mat4 viewMatrix{1.f};
    glm::mat4 invViewMatrix{1.f};
    unsigned int debugMode{0};
};

class RenderSystem {
public:
  RenderSystem(
    Device &passDevice,
    VkRenderPass renderPass,
    std::string dynamicShaderPath,
    const VkDescriptorSetLayout* globalSetLayout,
    const unsigned int setLayoutCount = 1,
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
  ~RenderSystem();

  RenderSystem(const RenderSystem &) = delete;
  RenderSystem &operator=(const RenderSystem &) = delete;
  
  void recreatePipeline(VkRenderPass renderPass, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
  virtual void renderSolidObjects(FrameInfo &frameInfo);
  virtual void renderSolidObjects(FrameInfoNoMaterials &frameInfo);
  
  Pipeline::Status getPipelineStatus(void) { return (pipeline != nullptr ? pipeline->getInternalStatus() : Pipeline::Status::ERR); }

private:
  void createPipelineLayout(const VkDescriptorSetLayout* globalSetLayout, const unsigned int setLayoutCount);
  void createPipeline(VkRenderPass renderPass);

protected:
    Device &device;

    std::unique_ptr<Pipeline> pipeline;
    VkPipelineLayout pipelineLayout;
    VkSampleCountFlagBits sampleCount;
    std::string shaderPath;
};

#endif /* RenderSystem_hpp */
