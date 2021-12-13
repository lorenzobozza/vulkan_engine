//
//  Descriptors.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 15/11/21.
//

#include "include/Descriptors.hpp"

// std
#include <cassert>
#include <stdexcept>
#include <iostream>

// *************** Descriptor Set Layout Builder *********************

DescriptorSetLayout::Builder &DescriptorSetLayout::Builder::addBinding(
    uint32_t binding,
    VkDescriptorType descriptorType,
    VkShaderStageFlags stageFlags,
    VkDescriptorBindingFlags bindingFlags,
    uint32_t count) {
  assert(bindings.count(binding) == 0 && "Binding already in use");
  VkDescriptorSetLayoutBinding layoutBinding{};
  layoutBinding.binding = binding;
  layoutBinding.descriptorType = descriptorType;
  layoutBinding.descriptorCount = count;
  layoutBinding.stageFlags = stageFlags;
  bindings[binding] = layoutBinding;
  bindingsFlags.push_back(bindingFlags);
  return *this;
}

std::unique_ptr<DescriptorSetLayout> DescriptorSetLayout::Builder::build() const {
  return std::make_unique<DescriptorSetLayout>(device, bindings, bindingsFlags);
}

// *************** Descriptor Set Layout *********************

DescriptorSetLayout::DescriptorSetLayout(
    Device &device,
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings,
    std::vector<VkDescriptorBindingFlags> bindingsFlags)
    : device{device}, bindings{bindings}, bindingsFlags{bindingsFlags} {
  std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
  for (auto kv : bindings) {
    setLayoutBindings.push_back(kv.second);
  }

    // Binding flags for indexing (textures)
    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags{};
    binding_flags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    binding_flags.bindingCount = (uint32_t) bindings.size();
    binding_flags.pBindingFlags = bindingsFlags.data();

  VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
  descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
  descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();
  descriptorSetLayoutInfo.pNext = &binding_flags;
  //descriptorSetLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

  if (vkCreateDescriptorSetLayout(
          device.device(),
          &descriptorSetLayoutInfo,
          nullptr,
          &descriptorSetLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

DescriptorSetLayout::~DescriptorSetLayout() {
  vkDestroyDescriptorSetLayout(device.device(), descriptorSetLayout, nullptr);
}

// *************** Descriptor Pool Builder *********************

DescriptorPool::Builder &DescriptorPool::Builder::addPoolSize(
    VkDescriptorType descriptorType, uint32_t count) {
  poolSizes.push_back({descriptorType, count});
  return *this;
}

DescriptorPool::Builder &DescriptorPool::Builder::setPoolFlags(
    VkDescriptorPoolCreateFlags flags) {
  poolFlags = flags;
  return *this;
}
DescriptorPool::Builder &DescriptorPool::Builder::setMaxSets(uint32_t count) {
  maxSets = count;
  return *this;
}

std::unique_ptr<DescriptorPool> DescriptorPool::Builder::build() const {
  return std::make_unique<DescriptorPool>(device, maxSets, poolFlags, poolSizes);
}

// *************** Descriptor Pool *********************

DescriptorPool::DescriptorPool(
    Device &device,
    uint32_t maxSets,
    VkDescriptorPoolCreateFlags poolFlags,
    const std::vector<VkDescriptorPoolSize> &poolSizes)
    : device{device} {
  VkDescriptorPoolCreateInfo descriptorPoolInfo{};
  descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  descriptorPoolInfo.pPoolSizes = poolSizes.data();
  descriptorPoolInfo.maxSets = maxSets;
  descriptorPoolInfo.flags = poolFlags;// | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

  if (vkCreateDescriptorPool(device.device(), &descriptorPoolInfo, nullptr, &descriptorPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }
}

DescriptorPool::~DescriptorPool() {
  vkDestroyDescriptorPool(device.device(), descriptorPool, nullptr);
}

bool DescriptorPool::allocateDescriptor(
    const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor, const std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> &bindings) const {
    
    uint32_t max_counts{1};
    for (auto binding : bindings) {
        max_counts = std::max(binding.second.descriptorCount, max_counts);
    }
    VkDescriptorSetVariableDescriptorCountAllocateInfo set_counts{};
    set_counts.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    set_counts.descriptorSetCount = 1;
    set_counts.pDescriptorCounts = &max_counts;
    
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptorPool;
  allocInfo.pSetLayouts = &descriptorSetLayout;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pNext = &set_counts;

  // Might want to create a "DescriptorPoolManager" class that handles this case, and builds
  // a new pool whenever an old pool fills up. But this is beyond our current scope
  if (vkAllocateDescriptorSets(device.device(), &allocInfo, &descriptor) != VK_SUCCESS) {
    return false;
  }
  return true;
}

void DescriptorPool::freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const {
  vkFreeDescriptorSets(
      device.device(),
      descriptorPool,
      static_cast<uint32_t>(descriptors.size()),
      descriptors.data());
}

void DescriptorPool::resetPool() {
  vkResetDescriptorPool(device.device(), descriptorPool, 0);
}

// *************** Descriptor Writer *********************

DescriptorWriter::DescriptorWriter(DescriptorSetLayout &setLayout, DescriptorPool &pool)
    : setLayout{setLayout}, pool{pool} {}

DescriptorWriter &DescriptorWriter::writeBuffer(
    uint32_t binding, VkDescriptorBufferInfo *bufferInfo) {
  assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");

  auto &bindingDescription = setLayout.bindings[binding];

  assert(
      bindingDescription.descriptorCount == 1 &&
      "Binding single descriptor info, but binding expects multiple");

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorType = bindingDescription.descriptorType;
  write.dstBinding = binding;
  write.pBufferInfo = bufferInfo;
  write.descriptorCount = 1;

  writes.push_back(write);
  return *this;
}

DescriptorWriter &DescriptorWriter::writeImage(
    uint32_t binding, VkDescriptorImageInfo *imageInfo) {
  assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");

  auto &bindingDescription = setLayout.bindings[binding];

  //assert(
  //    bindingDescription.descriptorCount == 2 &&
  //    "Binding single descriptor info, but binding expects multiple");

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorType = bindingDescription.descriptorType;
  write.dstBinding = binding;
  write.pImageInfo = imageInfo;
  write.descriptorCount = bindingDescription.descriptorCount;
  write.dstArrayElement = 0;

  writes.push_back(write);
  return *this;
}

bool DescriptorWriter::build(VkDescriptorSet &set) {
  bool success = pool.allocateDescriptor(setLayout.getDescriptorSetLayout(), set, setLayout.bindings);
  if (!success) {
    return false;
  }
  overwrite(set);
  return true;
}

void DescriptorWriter::overwrite(VkDescriptorSet &set) {
  for (auto &write : writes) {
    write.dstSet = set;
  }
  vkUpdateDescriptorSets(pool.device.device(), (uint32_t)writes.size(), writes.data(), 0, nullptr);
}
