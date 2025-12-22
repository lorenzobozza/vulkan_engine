//
//  CubeMap.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 30/12/21.
//

#ifndef CubeMap_hpp
#define CubeMap_hpp

#include "Buffer.hpp"
#include "Device.hpp"
#include "Descriptors.hpp"
#include "Image.hpp"
#include "Pipeline.hpp"

class CubeMap {
public:
    CubeMap(const Device& device, VkDescriptorImageInfo* srcDescriptor, VkExtent2D extent, std::string shader, uint16_t mipLevels = 1);
    ~CubeMap();
    
    VkDescriptorImageInfo* getImageDescriptor(void) { return &m_OffscreenPass.descriptor; }
    
private:
    constexpr static VkFormat FB_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
    
    struct CubeUbo {
        glm::mat4 projectionView{1.f};
        glm::mat4 viewMatrix{1.f};
        float roughness{1.f};
    };
    
    struct FrameBufferAttachment {
        VkImage image;
        VkDeviceMemory mem;
        VkImageView view;
    };
    struct OffscreenPass {
        int32_t width, height;
        VkFramebuffer frameBuffer;
        FrameBufferAttachment color, depth;
        VkRenderPass renderPass;
        VkSampler sampler;
        VkDescriptorImageInfo descriptor;
    } m_OffscreenPass;
    
    DescriptorStruct m_Descriptors;
    
    void initCubeMap(void);
    void renderFaces(void);
    void createDescriptorSets(void);
    void createPipelineLayout(void);
    void createPipeline(void);
    void createOffscreenRenderPass(void);
    void createOffscreenFramebuffer(void);
    void destroyFramebuffer(void);
    
    VkCommandBuffer beginFrame(void);
    void endFrame(void);
    void beginRenderPass(void);
    void endRenderPass(void);
    void createCommandBuffer(void);
    void freeCommandBuffer(void);
    
    static glm::mat4 lookAtFace(const uint16_t index);
    
    const Device& m_Device;
    Image m_Image{m_Device};
    
    std::unique_ptr<Pipeline> m_Pipeline;
    std::unique_ptr<Buffer> m_UboBuffer;
    
    VkDescriptorImageInfo* m_SrcDescriptor;
    VkExtent2D m_Extent;
    VkFormat m_DepthFormat;
    VkPipelineLayout m_PipelineLayout;
    VkCommandBuffer m_CommandBuffer;
    FrameBufferAttachment m_CubeMap;
    VkSampler m_CubeMapSampler;
    uint16_t m_MipLevels;
    bool m_IsFrameStarted = false;
    
    std::string m_ShaderName;
};

#endif /* CubeMap_hpp */
