//
//  Descriptors.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 15/11/21.
//

#include "include/Descriptors.hpp"

#include <cassert>
#include <stdexcept>


DescriptorSetLayout::DescriptorSetLayout(const Device& device, std::unordered_map<uint32_t,
                                         VkDescriptorSetLayoutBinding> bindings, std::vector<VkDescriptorBindingFlags> bindingsFlags)
: m_Device(device), m_Bindings(bindings) {
    
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
    for (auto kv : m_Bindings) {
        setLayoutBindings.push_back(kv.second);
    }
    for (size_t i = 0; i < (bindingsFlags.size() / 2U); ++i) {
        VkDescriptorBindingFlags temp = bindingsFlags[i];
        bindingsFlags[i] = bindingsFlags[bindingsFlags.size() - i - 1];
        bindingsFlags[bindingsFlags.size() - i - 1] = temp;
    }
    
    VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(setLayoutBindings.size()),
        .pBindingFlags = bindingsFlags.data(),
        .pNext = nullptr
    };
    
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(setLayoutBindings.size()),
        .pBindings = setLayoutBindings.data(),
        .pNext = &descriptorSetLayoutBindingFlagsInfo
    };
    
    if (vkCreateDescriptorSetLayout(m_Device.device(), &descriptorSetLayoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

DescriptorSetLayout::~DescriptorSetLayout() {
    vkDestroyDescriptorSetLayout(m_Device.device(), m_DescriptorSetLayout, nullptr);
}

DescriptorSetLayout::Builder& DescriptorSetLayout::Builder::addBinding(uint32_t binding, VkDescriptorType descriptorType,
                                                                       VkShaderStageFlags stageFlags, VkDescriptorBindingFlags bindingFlags,
                                                                       uint32_t count) {
    assert(m_Bindings.count(binding) == 0 && "Binding already in use");
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = descriptorType;
    layoutBinding.descriptorCount = count;
    layoutBinding.stageFlags = stageFlags;
    m_Bindings[binding] = layoutBinding;
    m_BindingsFlags.push_back(bindingFlags);
    return *this;
}

DescriptorSetLayout DescriptorSetLayout::Builder::build(void) const {
    return DescriptorSetLayout(m_Device, m_Bindings, m_BindingsFlags);
}

std::unique_ptr<DescriptorSetLayout> DescriptorSetLayout::Builder::build_ptr(void) const {
    return std::make_unique<DescriptorSetLayout>(m_Device, m_Bindings, m_BindingsFlags);
}




DescriptorPool::DescriptorPool(const Device& device, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags,
                               const std::vector<VkDescriptorPoolSize> &poolSizes) : m_Device(device) {
    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolInfo.pPoolSizes = poolSizes.data();
    descriptorPoolInfo.maxSets = maxSets;
    descriptorPoolInfo.flags = poolFlags;
    
    if (vkCreateDescriptorPool(m_Device.device(), &descriptorPoolInfo, nullptr, &m_DescriptorPool) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

DescriptorPool::~DescriptorPool() {
    vkDestroyDescriptorPool(m_Device.device(), m_DescriptorPool, nullptr);
}

bool DescriptorPool::allocateDescriptor(const VkDescriptorSetLayout* descriptorSetLayout, VkDescriptorSet& descriptor,
                                        const std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& bindings) const {
    
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.pSetLayouts = descriptorSetLayout;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pNext = nullptr;
    
    // Might want to create a "DescriptorPoolManager" class that handles this case, and builds
    // a new pool whenever an old pool fills up. But this is beyond our current scope
    if (vkAllocateDescriptorSets(m_Device.device(), &allocInfo, &descriptor) != VK_SUCCESS) {
        return false;
    }
    return true;
}

void DescriptorPool::freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const {
    vkFreeDescriptorSets(m_Device.device(), m_DescriptorPool, static_cast<uint32_t>(descriptors.size()), descriptors.data());
}

void DescriptorPool::resetPool(void) {
    vkResetDescriptorPool(m_Device.device(), m_DescriptorPool, 0);
}

DescriptorPool::Builder& DescriptorPool::Builder::addPoolSize(VkDescriptorType descriptorType, uint32_t count) {
    m_PoolSizes.push_back({descriptorType, count});
    return *this;
}

DescriptorPool::Builder& DescriptorPool::Builder::setPoolFlags(VkDescriptorPoolCreateFlags flags) {
    m_PoolFlags = flags;
    return *this;
}
DescriptorPool::Builder& DescriptorPool::Builder::setMaxSets(uint32_t count) {
    m_MaxSets = count;
    return *this;
}

DescriptorPool DescriptorPool::Builder::build(void) const {
    return DescriptorPool(m_Device, m_MaxSets, m_PoolFlags, m_PoolSizes);
}

std::unique_ptr<DescriptorPool> DescriptorPool::Builder::build_ptr(void) const {
    return std::make_unique<DescriptorPool>(m_Device, m_MaxSets, m_PoolFlags, m_PoolSizes);
}




DescriptorWriter::DescriptorWriter(DescriptorSetLayout &setLayout, DescriptorPool &pool)
: m_DescriptorSetLayout{setLayout}, m_DescriptorPool{pool} {}

DescriptorWriter &DescriptorWriter::writeBuffer(uint32_t binding, VkDescriptorBufferInfo *bufferInfo) {
    assert(m_DescriptorSetLayout.m_Bindings.count(binding) == 1 && "Layout does not contain specified binding");
    
    auto &bindingDescription = m_DescriptorSetLayout.m_Bindings[binding];
    
    assert(bindingDescription.descriptorCount == 1 && "Binding single descriptor info, but binding expects multiple");
    
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType = bindingDescription.descriptorType;
    write.dstBinding = binding;
    write.pBufferInfo = bufferInfo;
    write.descriptorCount = 1;
    
    m_WriteDescriptorSet.push_back(write);
    return *this;
}

DescriptorWriter& DescriptorWriter::writeImage(uint32_t binding, VkDescriptorImageInfo *imageInfo) {
    if (binding == UINT32_MAX) return *this;
    
    assert(m_DescriptorSetLayout.m_Bindings.count(binding) == 1 && "Layout does not contain specified binding");
    
    auto& bindingDescription = m_DescriptorSetLayout.m_Bindings[binding];
    
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType = bindingDescription.descriptorType;
    write.dstBinding = binding;
    write.pImageInfo = imageInfo;
    write.descriptorCount = bindingDescription.descriptorCount;
    write.dstArrayElement = 0;
    
    m_WriteDescriptorSet.push_back(write);
    return *this;
}

bool DescriptorWriter::build(VkDescriptorSet& set) {
    bool success = m_DescriptorPool.allocateDescriptor(m_DescriptorSetLayout.getDescriptorSetLayout(), set, m_DescriptorSetLayout.m_Bindings);
    if (!success) {
        return false;
    }
    overwrite(set);
    return true;
}

void DescriptorWriter::overwrite(VkDescriptorSet& set) {
    for (auto& write : m_WriteDescriptorSet) {
        write.dstSet = set;
    }
    vkUpdateDescriptorSets(m_DescriptorPool.m_Device.device(), (uint32_t)m_WriteDescriptorSet.size(), m_WriteDescriptorSet.data(), 0, nullptr);
}
