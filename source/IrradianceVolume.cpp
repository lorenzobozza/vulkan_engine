//
//  IrradianceVolume.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 24/02/26.
//


#include "Stopwatch.hpp"
#include "IrradianceVolume.hpp"
#include "Image.hpp"
#include "ScenePipeline.hpp"

#include "json.hpp"
#include "TaskScheduler.h"

#include <fstream>


extern IVUBO sbuffo;

IrradianceVolume::IrradianceVolume(Device& device, std::string inputFile) : m_Device(device) {
    Stopwatch sw;
    sw.begin();
    parseFromJsonFile(inputFile);
    createVoxelImage();
    sw.end("Irradiance Volume load time: ");
}

IrradianceVolume::~IrradianceVolume() {
    vkDestroySampler(m_Device.device(), m_VkSampler, nullptr);
    vkDestroyImageView(m_Device.device(), m_VkImageView, nullptr);
    vkDestroyImage(m_Device.device(), m_VkImage, nullptr);
    vkFreeMemory(m_Device.device(), m_VkDevMemory, nullptr);
}

void IrradianceVolume::parseFromJsonFile(std::string file) {
    nlohmann::json data;
    
    std::ifstream fs(file);
    fs >> data;
    fs.close();
    
    uint32_t bsizeX = data["sX"].get<uint32_t>();
    uint32_t bsizeY = data["sY"].get<uint32_t>();
    uint32_t bsizeZ = data["sZ"].get<uint32_t>();
    
    // Z <-> Y
    m_GridSize.x = bsizeX;
    m_GridSize.y = bsizeZ;
    m_GridSize.z = bsizeY;
    m_GridSize.total = bsizeX * bsizeY * bsizeZ;
    
    
    // Z -> -Y, Y -> Z
    glm::vec3 translation(data["location"][0].get<float>(), -data["location"][2].get<float>(), data["location"][1].get<float>());
    glm::vec3 rotate(data["rotation"][0].get<float>(), -data["rotation"][2].get<float>(), data["rotation"][1].get<float>());
    glm::vec3 scale(data["scale"][0].get<float>(), data["scale"][2].get<float>(), data["scale"][1].get<float>());
    
    sbuffo.invTransform = glm::inverse(glm::translate(glm::mat4(1.0), translation) * glm::scale(glm::mat4(1.0), scale));
    sbuffo.res.x = m_GridSize.x;
    sbuffo.res.y = m_GridSize.y;
    sbuffo.res.z = m_GridSize.z;
    
    if(data["SH"].is_array()) {
        VkDeviceSize imageSize = m_GridSize.total * m_ShCoefficients * 16U;

        m_StagingBuffer = std::make_unique<Buffer>(m_Device, imageSize, 1,
                                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        m_StagingBuffer->map();

        glm::vec4* sphericalHarmonics = (glm::vec4*)m_StagingBuffer->getMappedMemory();
        
        uint32_t i = 0;
        for(auto& sh : data["SH"]) {
            // Re-order grid to match vulkan basis
            uint32_t ix = i % bsizeX;
            uint32_t iy = (i / bsizeX) % bsizeY;
            uint32_t iz = i / (bsizeX * bsizeY);
            iz = bsizeZ - iz - 1;
            uint32_t j = ix + (iz * bsizeX) + (iy * bsizeX * bsizeZ);
            
            // Z -> -Y, Y -> Z
            
            sphericalHarmonics[j + (m_GridSize.total * 0)] = glm::vec4(sh["L0"][0].get<float>(),
                                                                         sh["L0"][1].get<float>(),
                                                                         sh["L0"][2].get<float>(),
                                                                         sh["L0"][3].get<float>());
            
            sphericalHarmonics[j + (m_GridSize.total * 1)] = glm::vec4(sh["L1b"][0].get<float>(),
                                                                         sh["L1b"][1].get<float>(),
                                                                         sh["L1b"][2].get<float>(),
                                                                         sh["L1b"][3].get<float>());
            
            sphericalHarmonics[j + (m_GridSize.total * 2)] = -glm::vec4(sh["L1a"][0].get<float>(),
                                                                          sh["L1a"][1].get<float>(),
                                                                          sh["L1a"][2].get<float>(),
                                                                          sh["L1a"][3].get<float>());
            
            sphericalHarmonics[j + (m_GridSize.total * 3)] = glm::vec4(sh["L1c"][0].get<float>(),
                                                                         sh["L1c"][1].get<float>(),
                                                                         sh["L1c"][2].get<float>(),
                                                                         sh["L1c"][3].get<float>());
            ++i;
        }
        
    }
}

void IrradianceVolume::createVoxelImage(void) {
    VkExtent3D extent { .width  = m_GridSize.x, .height = m_GridSize.y, .depth  = m_GridSize.z * m_ShCoefficients };

    const int memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    const int memoryUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    
    Image img(m_Device);
    img.createImage(extent, VK_IMAGE_TYPE_3D, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
                    memoryUsage, memoryFlags, m_VkImage, m_VkDevMemory);
    

    auto cmbf = img.beginSingleTimeCommands();
    img.transitionImageLayout(cmbf, m_VkImage, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    img.copyBufferToImage(cmbf, m_StagingBuffer->getBuffer(), m_VkImage, extent);
    img.transitionImageLayout(cmbf, m_VkImage, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    img.endSingleTimeCommands(cmbf);

    m_StagingBuffer.reset();
    
    m_VkImageView = img.createImageView(m_VkImage, VK_IMAGE_VIEW_TYPE_3D, VK_FORMAT_R32G32B32A32_SFLOAT);
    
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        
    samplerInfo.anisotropyEnable = VK_FALSE;
    
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    
    if (vkCreateSampler(m_Device.device(), &samplerInfo, nullptr, &m_VkSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
    
    m_VkDescriptorImageInfo.sampler = m_VkSampler;
    m_VkDescriptorImageInfo.imageView = m_VkImageView;
    m_VkDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    m_Device.ext->setDebugUtilsObjectName(VK_OBJECT_TYPE_IMAGE, (uint64_t)m_VkImage, "IrradianceVolume");
}

