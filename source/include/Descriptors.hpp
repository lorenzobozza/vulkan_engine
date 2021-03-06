//
//  Descriptors.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 15/11/21.
//

#ifndef Descriptors_hpp
#define Descriptors_hpp

#include "Device.hpp"
#include "SwapChain.hpp"

// std
#include <memory>
#include <unordered_map>
#include <vector>

class DescriptorSetLayout {
 public:
  class Builder {
   public:
    Builder(Device &device) : device{device} {}

    Builder &addBinding(
        uint32_t binding,
        VkDescriptorType descriptorType,
        VkShaderStageFlags stageFlags,
        VkDescriptorBindingFlags bindingFlags = 0,
        uint32_t count = 1);
    std::unique_ptr<DescriptorSetLayout> build() const;

   private:
    Device &device;
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings{};
    std::vector<VkDescriptorBindingFlags> bindingsFlags{};
    
  };

  DescriptorSetLayout(
      Device &device,
      std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings,
      std::vector<VkDescriptorBindingFlags> bindingsFlags);
  ~DescriptorSetLayout();
  DescriptorSetLayout(const DescriptorSetLayout &) = delete;
  DescriptorSetLayout &operator=(const DescriptorSetLayout &) = delete;

  VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }

 private:
  Device &device;
  VkDescriptorSetLayout descriptorSetLayout;
  std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;
  std::vector<VkDescriptorBindingFlags> bindingsFlags;

  friend class DescriptorWriter;
};

class DescriptorPool {
 public:
  class Builder {
   public:
    Builder(Device &device) : device{device} {}

    Builder &addPoolSize(VkDescriptorType descriptorType, uint32_t count);
    Builder &setPoolFlags(VkDescriptorPoolCreateFlags flags);
    Builder &setMaxSets(uint32_t count);
    std::unique_ptr<DescriptorPool> build() const;

   private:
    Device &device;
    std::vector<VkDescriptorPoolSize> poolSizes{};
    uint32_t maxSets = 1000;
    VkDescriptorPoolCreateFlags poolFlags = 0;
  };

  DescriptorPool(
      Device &device,
      uint32_t maxSets,
      VkDescriptorPoolCreateFlags poolFlags,
      const std::vector<VkDescriptorPoolSize> &poolSizes);
  ~DescriptorPool();
  DescriptorPool(const DescriptorPool &) = delete;
  DescriptorPool &operator=(const DescriptorPool &) = delete;

  bool allocateDescriptor(
      const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor, const std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> &bindings) const;

  void freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const;

  void resetPool();

 private:
  Device &device;
  VkDescriptorPool descriptorPool;

  friend class DescriptorWriter;
};

class DescriptorWriter {
 public:
  DescriptorWriter(DescriptorSetLayout &setLayout, DescriptorPool &pool);

  DescriptorWriter &writeBuffer(uint32_t binding, VkDescriptorBufferInfo *bufferInfo);
  DescriptorWriter &writeImage(uint32_t binding, VkDescriptorImageInfo *imageInfo);

  bool build(VkDescriptorSet &set);
  void overwrite(VkDescriptorSet &set);

 private:
  DescriptorSetLayout &setLayout;
  DescriptorPool &pool;
  std::vector<VkWriteDescriptorSet> writes;
};

#endif /* Descriptors_hpp */
