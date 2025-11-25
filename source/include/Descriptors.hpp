//
//  Descriptors.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 15/11/21.
//

#ifndef Descriptors_hpp
#define Descriptors_hpp

#include "Device.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

class DescriptorSetLayout {
public:
    DescriptorSetLayout(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;
    
    DescriptorSetLayout(const Device& device,
                        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings,
                        std::vector<VkDescriptorBindingFlags> bindingsFlags);
    ~DescriptorSetLayout();
    
    const VkDescriptorSetLayout* getDescriptorSetLayout() const { return &m_DescriptorSetLayout; }
    
    class Builder {
    public:
        Builder(const Device& device) : m_Device(device) {}
        
        Builder& addBinding(uint32_t binding,
                            VkDescriptorType descriptorType,
                            VkShaderStageFlags stageFlags,
                            VkDescriptorBindingFlags bindingFlags = 0,
                            uint32_t count = 1);
        
        DescriptorSetLayout build(void) const;
        std::unique_ptr<DescriptorSetLayout> build_ptr(void) const;
        
    private:
        const Device& m_Device;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> m_Bindings;
        std::vector<VkDescriptorBindingFlags> m_BindingsFlags;
        
    };
    
private:
    const Device& m_Device;
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> m_Bindings;
    std::vector<VkDescriptorBindingFlags> m_BindingsFlags;
    VkDescriptorSetLayout m_DescriptorSetLayout;

    friend class DescriptorWriter;
};


class DescriptorPool {
public:
    DescriptorPool(const DescriptorPool&) = delete;
    DescriptorPool& operator=(const DescriptorPool&) = delete;
    
    DescriptorPool(const Device& device, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags, const std::vector<VkDescriptorPoolSize>& poolSizes);
    ~DescriptorPool();
    
    bool allocateDescriptor(const VkDescriptorSetLayout* descriptorSetLayout, VkDescriptorSet &descriptor,
                            const std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> &bindings) const;
    void freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const;
    void resetPool(void);
    
    class Builder {
    public:
        Builder(const Device& device) : m_Device(device) {}
        
        Builder& addPoolSize(VkDescriptorType descriptorType, uint32_t count);
        Builder& setPoolFlags(VkDescriptorPoolCreateFlags flags);
        Builder& setMaxSets(uint32_t count);
        DescriptorPool build(void) const;
        std::unique_ptr<DescriptorPool> build_ptr(void) const;
        
    private:
        const Device& m_Device;
        std::vector<VkDescriptorPoolSize> m_PoolSizes;
        uint32_t m_MaxSets = 1000;
        VkDescriptorPoolCreateFlags m_PoolFlags = 0;
    };
    
private:
    const Device& m_Device;
    VkDescriptorPool m_DescriptorPool;
    
    friend class DescriptorWriter;
};


class DescriptorWriter {
public:
    DescriptorWriter(DescriptorSetLayout& setLayout, DescriptorPool& pool);
    
    DescriptorWriter& writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo);
    DescriptorWriter& writeImage(uint32_t binding, VkDescriptorImageInfo* imageInfo);
    
    bool build(VkDescriptorSet& set);
    void overwrite(VkDescriptorSet& set);
    
private:
    DescriptorSetLayout& m_DescriptorSetLayout;
    DescriptorPool& m_DescriptorPool;
    std::vector<VkWriteDescriptorSet> m_WriteDescriptorSet;
};

typedef struct DescriptorStruct_s {
    std::unique_ptr<DescriptorSetLayout> layout;
    std::unique_ptr<DescriptorPool> pool;
    std::vector<VkDescriptorSet> v_set;
} DescriptorStruct;

#endif /* Descriptors_hpp */
