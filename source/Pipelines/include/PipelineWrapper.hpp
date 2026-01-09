//
//  PipelineWrapper.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 12/11/25.
//

#ifndef PipelineWrapper_hpp
#define PipelineWrapper_hpp

#include <array>
#include <vector>
#include <string>
#include <mutex>

#include "Device.hpp"
#include "Pipeline.hpp"
#include "SwapChain.hpp"
#include "Descriptors.hpp"

class PipelineWrapper {
    
public:
    virtual ~PipelineWrapper();
    
    PipelineWrapper(const PipelineWrapper&) = delete;
    PipelineWrapper& operator=(const PipelineWrapper&) = delete;
    PipelineWrapper(PipelineWrapper&&) = delete;
    PipelineWrapper& operator=(PipelineWrapper&&) = delete;
    
    virtual void render(VkCommandBuffer commandBuffer, int frameIndex) = 0;
    virtual void safe_render(VkCommandBuffer commandBuffer, int frameIndex) final;
    virtual void recreatePipeline(VkRenderPass renderPass = VK_NULL_HANDLE, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM) final;
    
    virtual void setMultiSampling(VkSampleCountFlagBits samples) final { m_SampleCount = samples; }
    virtual Pipeline::Status getPipelineStatus(void) final {return (m_Pipeline != nullptr ? m_Pipeline->getInternalStatus() : Pipeline::Status::ERR);}
    
protected:
    PipelineWrapper(const Device& device, VkRenderPass renderPass, std::string shader, std::string _shader = "NULL")
    : m_Device(device), m_RenderPass(renderPass), str_vert(shader), str_frag(_shader) {}
    virtual void _inheritedConstructor(void) final;
    
    virtual void createPipeline(void);
    virtual void destroyPipeline(void);
    virtual void customizePipelineConfig(PipelineConfigInfo& config) = 0;
    
    struct Dependencies {
        const std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
        const std::vector<VkPushConstantRange> pushConstantRanges;
    };
    virtual void createPipelineLayout(void) final;
    virtual void destroyPipelineLayout(void) final;
    virtual Dependencies createLayoutDependencies(void) = 0;
    virtual void beforeRecreate(void) {}
    
    const Device& m_Device;
    
    VkRenderPass m_RenderPass;
    VkPipelineLayout m_PipelineLayout;
    std::unique_ptr<Pipeline> m_Pipeline;
    
    VkSampleCountFlagBits m_SampleCount{VK_SAMPLE_COUNT_1_BIT};
    
    std::string str_vert, str_frag;
};

#endif /* PipelineWrapper_hpp */
