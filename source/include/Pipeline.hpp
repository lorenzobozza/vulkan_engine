//
//  Pipeline.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 05/11/21.
//

#ifndef Pipeline_hpp
#define Pipeline_hpp

#include "Device.hpp"

#include <string>

struct PipelineConfigInfo {
    PipelineConfigInfo(const PipelineConfigInfo&) = delete;
    PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;
    PipelineConfigInfo() = default;
    
    VkPipelineViewportStateCreateInfo viewportInfo = {};
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {};
    VkPipelineRasterizationStateCreateInfo rasterizationInfo = {};
    VkPipelineMultisampleStateCreateInfo multisampleInfo = {};
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    VkPipelineColorBlendStateCreateInfo colorBlendInfo = {};
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {};
    std::vector<VkDynamicState> dynamicStateEnables = {};
    VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpass = 0;
    //    VkPipelineRasterizationLineStateCreateInfo lineRasterizationInfo;
};

class Pipeline {
public:
    Pipeline(const Pipeline&) = delete;
    Pipeline &operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = delete;
    Pipeline &operator=(Pipeline&&) = delete;
    
    Pipeline(const Device& dev, const std::string &vertFilepath, const std::string &fragFilepath, const PipelineConfigInfo &configInfo);
    ~Pipeline();
    
    enum Status {
        OK = 0,
        ERR
    };
    
    void bind(VkCommandBuffer commandBuffer);
    Status getInternalStatus(void) const { return m_InternalStatus; }
    static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);
    
private:
    Status createGraphicsPipeline(const std::string &vertFilepath, const std::string &fragFilepath, const PipelineConfigInfo &configInfo);
    void createShaderModule(std::vector<uint32_t>& vecShader, VkShaderModule *shaderModule);
    
    const Device& m_Device;
    VkPipeline m_GraphicsPipeline;
    VkShaderModule m_VertShaderModule;
    VkShaderModule m_FragShaderModule;
    Status m_InternalStatus;
};

#endif /* Pipeline_hpp */
