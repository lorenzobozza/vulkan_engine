//
//  IrradianceVolume.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 25/02/26.
//

#ifndef IrradianceVolume_hpp
#define IrradianceVolume_hpp

#include "Device.hpp"
#include "Buffer.hpp"

class IrradianceVolume {
public:
    IrradianceVolume(Device& device, std::string inputFile);
    ~IrradianceVolume();
    
    IrradianceVolume(const IrradianceVolume&) = delete;
    IrradianceVolume& operator=(const IrradianceVolume&) = delete;
    IrradianceVolume(IrradianceVolume&&) = delete;
    IrradianceVolume& operator=(IrradianceVolume&&) = delete;
    
    VkDescriptorImageInfo* getDescriptorImageInfo(void) { return &m_VkDescriptorImageInfo; }
    
private:
    Device& m_Device;
    
    const uint8_t m_ShCoefficients = 4U;
    struct {
        uint32_t x, y, z, total;
    } m_GridSize;
    
    std::unique_ptr<Buffer> m_StagingBuffer;
    VkImage m_VkImage;
    VkDeviceMemory m_VkDevMemory;
    VkImageView m_VkImageView;
    VkSampler m_VkSampler;
    VkDescriptorImageInfo m_VkDescriptorImageInfo;
    
    void parseFromJsonFile(std::string file);
    void createVoxelImage(void);
};


#endif
