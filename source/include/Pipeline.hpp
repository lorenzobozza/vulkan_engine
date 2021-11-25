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
    //PipelineConfigInfo() = default;
    PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;
    
    //VkViewport viewport;
    //VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewportInfo;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    VkPipelineRasterizationStateCreateInfo rasterizationInfo;
    VkPipelineMultisampleStateCreateInfo multisampleInfo;
    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo colorBlendInfo;
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
    std::vector<VkDynamicState> dynamicStateEnables;
    VkPipelineDynamicStateCreateInfo dynamicStateInfo;
    VkPipelineLayout pipelineLayout = nullptr;
    VkRenderPass renderPass = nullptr;
    uint32_t subpass = 0;
};

class Pipeline {
public:
    Pipeline(Device &dev, const std::string &vertFilepath, const std::string &fragFilepath, const PipelineConfigInfo &configInfo);
    
    ~Pipeline();
    
    // Prevent Obj copy
    Pipeline(const Pipeline &) = delete;
    Pipeline &operator=(const Pipeline &) = delete;
    
    void bind(VkCommandBuffer commandBuffer);

    static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);
    
private:
    static std::vector<char> readFile(const std::string &filepath);
    
    void createGraphicsPipeline(const std::string &vertFilepath, const std::string &fragFilepath, const PipelineConfigInfo &configInfo);
    
    void createShaderModule(const std::vector<char> &code, VkShaderModule *shaderModule);
    
    Device &device;
    VkPipeline graphicsPipeline;
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
};

#endif /* Pipeline_hpp */
