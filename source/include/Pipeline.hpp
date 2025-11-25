//
//  Pipeline.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 05/11/21.
//

#ifndef Pipeline_hpp
#define Pipeline_hpp

#include "Device.hpp"
#include "Model.hpp"

// std headers
#include <string>
#include <vector>

struct PipelineConfigInfo {
    PipelineConfigInfo(const PipelineConfigInfo&) = delete;
    PipelineConfigInfo() = default;
    PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;
    
    //VkViewport viewport;
    //VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewportInfo;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    VkPipelineRasterizationStateCreateInfo rasterizationInfo;
//    VkPipelineRasterizationLineStateCreateInfo lineRasterizationInfo;
    VkPipelineMultisampleStateCreateInfo multisampleInfo;
    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo colorBlendInfo;
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
    std::vector<VkDynamicState> dynamicStateEnables;
    VkPipelineDynamicStateCreateInfo dynamicStateInfo;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpass = 0;
};

class Pipeline {
public:
    Pipeline(const Device& dev, const std::string &vertFilepath, const std::string &fragFilepath, const PipelineConfigInfo &configInfo);
    
    ~Pipeline();
    
    // Prevent Obj copy
    Pipeline(const Pipeline &) = delete;
    Pipeline &operator=(const Pipeline &) = delete;
    
    enum Status {
        OK = 0,
        ERR
    };
    
    void bind(VkCommandBuffer commandBuffer);
    
    Status getInternalStatus(void) { return m_internalStatus; }

    static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);
    
private:
    Status createGraphicsPipeline(const std::string &vertFilepath, const std::string &fragFilepath, const PipelineConfigInfo &configInfo);
    
    void createShaderModule(std::vector<uint32_t>& vecShader, VkShaderModule *shaderModule);
    
    const Device& device;
    VkPipeline graphicsPipeline;
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    
    Status m_internalStatus;
};

#endif /* Pipeline_hpp */
