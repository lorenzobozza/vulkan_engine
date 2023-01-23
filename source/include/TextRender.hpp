//
//  TextRender.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 10/12/21.
//

#ifndef TextRender_hpp
#define TextRender_hpp

#include "Device.hpp"
#include "Buffer.hpp"
#include "Descriptors.hpp"
#include "Pipeline.hpp"
#include "SolidObject.hpp"
#include "FrameInfo.hpp"
#include "Image.hpp"

// lib
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

//std
#include <vector>
#include <string>
#include <unordered_map>

class TextRender {
public:

    struct Character {
        unsigned int TextureID;  // ID handle of the glyph texture
        glm::ivec2   Size;       // Size of glyph
        glm::ivec2   Bearing;    // Offset from baseline to left/top of glyph
        long Advance;    // Offset to advance to next glyph
        glm::vec2   uv;
    };

    TextRender(Device &device, VkRenderPass renderPass, const char* fontPath);
    ~TextRender();
    
    unsigned int renderText(std::string text, float x, float y, float scale, glm::vec3 color = glm::vec3{1.f}, float aspect = 1.f);
    void render(VkCommandBuffer commandBuffer, int frameIndex);

private:

    Device &device;
    SolidObject::Map meshes;
    Image vulkanImage{device};
    std::unordered_map<char, Character> characters;

    void loadFaces(const char firstChar, const char lastChar);
    void createImageStack();
    void createSampler();
    void createDescriptors();
    void createPipelineLayout();
    void createPipeline(VkRenderPass renderPass);

    unsigned char* bitmaps;
    uint16_t layers;
    uint32_t bitmapSize;
    uint32_t bitmapArea;
    
    std::unique_ptr<DescriptorPool> textPool;
    std::unique_ptr<DescriptorSetLayout> textSetLayout;
    std::vector<VkDescriptorSet> *textDescriptorSets;
    std::unique_ptr<Pipeline> pipeline;
    VkPipelineLayout pipelineLayout;
    
    VkImage bitmapImage{};
    VkDeviceMemory bitmapImageMemory{};
    VkImageView bitmapImageView{};
    VkSampler bitmapSampler{};
    
    const char* fontPath;
};

#endif /* TextRender_hpp */
