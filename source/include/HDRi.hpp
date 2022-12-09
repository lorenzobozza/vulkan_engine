//
//  HDRi.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 30/12/21.
//

#ifndef HDRi_hpp
#define HDRi_hpp

#include "Renderer.hpp"
#include "Descriptors.hpp"
#include "Camera.hpp"
#include "SolidObject.hpp"
#include "FrameInfo.hpp"
#include "Texture.hpp"
#include "RenderSystem.hpp"

class HDRi {
public:
    HDRi(Device &device, VkDescriptorImageInfo &srcDescriptor, VkExtent2D extent, std::string shader, uint16_t mipLevels = 1);
    ~HDRi();
    
    VkDescriptorImageInfo descriptorInfo();
    
private:
    static constexpr VkFormat FB_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
    
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
	} offscreenPass;
 
    struct Descriptor {
        std::unique_ptr<DescriptorPool> pool;
        std::unique_ptr<DescriptorSetLayout> setLayout;
        VkDescriptorSet set;
    } descriptor;
    
    struct CubeUbo {
        glm::mat4 projectionView{1.f};
        glm::mat4 viewMatrix{1.f};
        float roughness{1.f};
    };
    
    void initHDRi();
    void destroyFramebuffer();
    void renderFaces();
    void createDescriptorSets();
    void createPipelineLayout();
    void createPipeline();
    void createOffscreenRenderPass();
    void createOffscreenFramebuffer();
    
    VkCommandBuffer beginFrame();
    void endFrame();
    void beginRenderPass();
    void endRenderPass();
    void createCommandBuffer();
    void freeCommandBuffer();
    
    static glm::mat4 lookAtFace(const uint16_t index);
    
    Device &device;
    Camera cubeCamera{};
    Image vulkanImage{device};
    
    std::unique_ptr<Buffer> uboBuffer;
    std::unique_ptr<Pipeline> pipeline;
    
    VkDescriptorImageInfo &srcDescriptor;
    VkExtent2D extent;
    VkFormat depthFormat;
    VkPipelineLayout pipelineLayout;
    VkCommandBuffer commandBuffer;
    
    VkDescriptorImageInfo equirectangularMap;
    FrameBufferAttachment cubeMap{};
    VkSampler cubeSampler;
    
    std::string shader;
    uint16_t mipLevels;
    bool isFrameStarted = false;

};

#endif /* HDRi_hpp */
