//
//  TextRender.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 10/12/21.
//

#include "include/TextRender.hpp"

// lib
#include <ft2build.h>
#include FT_FREETYPE_H

TextRender::TextRender(Device &device, SolidObject::Map &meshes, const char* fontPath)
    : device{device}, meshes{meshes}, fontPath{fontPath} {
    loadFaces('!', '~'); // all faces from ! to ~
    createImageStack();
    createSampler();
}

TextRender::~TextRender() {
    vkDestroySampler(device.device(), bitmapSampler, nullptr);
    vkDestroyImageView(device.device(), bitmapImageView, nullptr);
    vkDestroyImage(device.device(), bitmapImage, nullptr);
    vkFreeMemory(device.device(), bitmapImageMemory, nullptr);
}

unsigned int TextRender::renderText(std::string text, float x, float y, float scale, glm::vec3 color, float aspect) {
    float startingXpos = x;
    float maxHeight{0};
    std::vector<unsigned int> boxes;
    scale *= .1f;
    std::string::const_iterator c;
    for (c = text.begin(); c != text.end(); c++)
    {
        if (*c != 32) {
            Character ch = characters[*c];

            float xpos = + (ch.Bearing.x / 100.f) * scale * (1.f / aspect);
            float ypos = - (ch.Bearing.y / 100.f) * scale;

            float w = (ch.Size.x / 100.f) * scale * (1.f / aspect);
            float h = (ch.Size.y / 100.f) * scale;
            
            Model::Data rectMesh{};
            rectMesh.vertices = {
                {{xpos,     ypos + h, 0.f}, color, {0.f, 0.f, -1.f}, {0.f,      1.f}},
                {{xpos + w, ypos + h, 0.f}, color, {0.f, 0.f, -1.f}, {ch.uv.x,  1.f}},
                {{xpos + w, ypos    , 0.f}, color, {0.f, 0.f, -1.f}, {ch.uv.x,  ch.uv.y}},
                {{xpos,     ypos    , 0.f}, color, {0.f, 0.f, -1.f}, {0.f,      ch.uv.y}}
            };
            
            rectMesh.indices = {0,3,2,0,2,1};
        
            auto rect = SolidObject::createSolidObject();
            rect.model = std::make_shared<Model>(device, rectMesh);
            rect.transform.translation = {x, y, 0.f};
            rect.textureIndex = ch.TextureID;
            
            meshes.emplace(rect.getId(), std::move(rect));
            boxes.push_back(rect.getId());
            
            if(ypos < maxHeight) { maxHeight = ypos; } //

            x += (ch.Advance >> 6) / 100.f * scale * (1.f / aspect);; // bitshift by 6 to get value in pixels (2^6 = 64)
        } else {
            x += 15 / 35.f * scale * (1.f / aspect);
        }
    }
    // Center text to given coordinates
    for (auto box : boxes) {
        meshes.at(box).transform.translation += glm::vec3(-.5f*(x - startingXpos), .5f*maxHeight, .0f);
    }
    return boxes.back();
}

VkDescriptorImageInfo TextRender::getDescriptor() {
    return VkDescriptorImageInfo {
        bitmapSampler,
        bitmapImageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
}

void TextRender::loadFaces(const char firstChar, const char lastChar) {
    layers = lastChar - firstChar;

    FT_Library ft;
    FT_Init_FreeType(&ft);
    
    FT_Face face;
    FT_New_Face(ft, fontPath, 0, &face);
    FT_Set_Pixel_Sizes(face, 0, 96);
    
    FT_Load_Char(face, 'W', FT_LOAD_RENDER);
    bitmapSize = face->glyph->bitmap.width;
    FT_Load_Char(face, 'l', FT_LOAD_RENDER);
    bitmapArea = face->glyph->bitmap.rows;
    FT_Load_Char(face, '(', FT_LOAD_RENDER);
    bitmapSize = std::max(bitmapSize, bitmapArea);
    const uint16_t maxSize = std::max(face->glyph->bitmap.rows, bitmapSize);
    bitmapSize = maxSize;
    const uint32_t maxArea = maxSize * maxSize;
    bitmapArea = maxArea;

    bitmaps = (unsigned char*) malloc(sizeof(unsigned char) * maxArea * layers);
    
    for(unsigned int c = firstChar; c < lastChar; c++) {
        FT_Load_Char(face, c, FT_LOAD_RENDER);
        int charWidth = face->glyph->bitmap.width;
        int charHeight = face->glyph->bitmap.rows;
        
        for (int i = 0, row = 0, charI = 0; i < maxArea; i++) {
            int col = (i % maxSize);
            if (col < charWidth && row >= (maxSize - charHeight)) {
                bitmaps[i+((c - firstChar) * maxArea)] = face->glyph->bitmap.buffer[charI++];
            } else {
                bitmaps[i+((c - firstChar) * maxArea)] = 0;
            }
            if (col == maxSize - 1) {
                row++;
            }
        }
        
        Character character = {
            c - firstChar,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            face->glyph->advance.x,
            glm::vec2{charWidth / (float)maxSize, (maxSize - charHeight) / (float)maxSize}
        };
        characters.insert(std::pair<char, Character>(c, character));
    }

    FT_Done_Face(face);
    
    FT_Done_FreeType(ft);
}

void TextRender::createImageStack() {
    VkDeviceSize imageSize = bitmapSize * bitmapSize * layers;
    
    if (!bitmaps) {
        throw std::runtime_error("Failed to load bitmap dataframes!");
    }
    
    Buffer stagingBuffer{
        device,
        imageSize,
        1,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };
    
    stagingBuffer.map();
    stagingBuffer.writeToBuffer(bitmaps);
    
    free(bitmaps);
    
    vulkanImage.createImage(
        bitmapSize, bitmapSize,
        VK_FORMAT_R8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        bitmapImage,
        bitmapImageMemory,
        layers);
        
    auto commandBuffer = vulkanImage.beginSingleTimeCommands();
    
    vulkanImage.transitionImageLayout(
        commandBuffer,
        bitmapImage,
        VK_FORMAT_R8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        layers);
    
    vulkanImage.copyBufferToImage(
        commandBuffer,
        stagingBuffer.getBuffer(),
        bitmapImage,
        bitmapSize,
        bitmapSize,
        layers);
    
    vulkanImage.transitionImageLayout(
        commandBuffer,
        bitmapImage,
        VK_FORMAT_R8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        layers);
        
    vulkanImage.endSingleTimeCommands(commandBuffer);
        
    bitmapImageView = vulkanImage.createImageView(bitmapImage, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_FORMAT_R8_SRGB, layers);
}

void TextRender::createSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    
    samplerInfo.anisotropyEnable = VK_FALSE;
    
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    
    if (vkCreateSampler(device.device(), &samplerInfo, nullptr, &bitmapSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create bitmap sampler!");
    }
}
