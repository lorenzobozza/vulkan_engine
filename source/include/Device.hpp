//
//  Device.hpp
//  vulkan_engine
//
//  Created by Brendan Galea on 05/11/21.
//

#ifndef Device_hpp
#define Device_hpp

#include "SDLWindow.hpp"

// std lib headers
#include <string>
#include <vector>
#include <stdexcept>


struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

//TODO: Add compute queue
struct QueueFamilyIndices {
  uint32_t graphicsFamily;
  uint32_t graphicsQueueCount;
  uint32_t transferFamily;
  uint32_t transferQueueCount;
  uint32_t presentFamily;
  uint32_t presentQueueCount;
  bool graphicsFamilyHasValue = false;
  bool transferFamilyHasValue = false;
  bool presentFamilyHasValue = false;
  bool isComplete() { return graphicsFamilyHasValue && transferFamilyHasValue && presentFamilyHasValue; }
};

class Device {
 public:
#ifdef NDEBUG
  const bool enableValidationLayers = false;
#else
  const bool enableValidationLayers = true;
#endif

  Device(SDLWindow &window);
  ~Device();

  // Not copyable or movable
  Device(const Device &) = delete;
  Device &operator=(const Device &) = delete;
  Device(Device &&) = delete;
  Device &operator=(Device &&) = delete;

  VkCommandPool getCommandPool() { return commandPool; }
  VkDevice device() { return device_; }
  VkSurfaceKHR surface() { return surface_; }
  VkQueue graphicsQueue() { return graphicsQueue_; }
  VkQueue transferQueue() { return transferQueue_; }
  VkQueue presentQueue() { return presentQueue_; }
  QueueFamilyIndices getFamilyIndices() { return indices; }

  SwapChainSupportDetails getSwapChainSupport() { return querySwapChainSupport(physicalDevice); }
  uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
  QueueFamilyIndices findPhysicalQueueFamilies() { return findQueueFamilies(physicalDevice); }
  VkFormat findSupportedFormat(
      const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

  // Buffer Helper Functions
  void createBuffer(
      VkDeviceSize size,
      VkBufferUsageFlags usage,
      VkMemoryPropertyFlags properties,
      VkBuffer &buffer,
      VkDeviceMemory &bufferMemory);
  VkCommandBuffer beginSingleTimeCommands();
  void endSingleTimeCommands(VkCommandBuffer commandBuffer);
  void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
  void copyBufferToImage(
      VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount);

  void createImageWithInfo(
      const VkImageCreateInfo &imageInfo,
      VkMemoryPropertyFlags properties,
      VkImage &image,
      VkDeviceMemory &imageMemory);

  VkPhysicalDeviceProperties properties;
  VkSampleCountFlagBits msaaSamples;
  VkSampleCountFlagBits maxSampleCount;

 private:
  void createInstance();
  void setupDebugMessenger();
  void createSurface();
  void pickPhysicalDevice();
  void createLogicalDevice();
  void createCommandPool();

  // helper functions
  VkSampleCountFlagBits getMaxUsableSampleCount();
  bool isDeviceSuitable(VkPhysicalDevice device);
  std::vector<const char *> getRequiredExtensions();
  bool checkValidationLayerSupport();
  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
  void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);
  void hasRequiredInstanceExtensions();
  bool checkDeviceExtensionSupport(VkPhysicalDevice device);
  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

  VkInstance instance;
  VkDebugUtilsMessengerEXT debugMessenger;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  SDLWindow &window;
  VkCommandPool commandPool;
  QueueFamilyIndices indices;

  VkDevice device_;
  VkSurfaceKHR surface_;
  VkQueue graphicsQueue_;
  VkQueue transferQueue_;
  VkQueue presentQueue_;
  

  const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
  const std::vector<const char *> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      //VK_KHR_MAINTENANCE3_EXTENSION_NAME,
      //VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
      #ifdef __APPLE__
      "VK_KHR_portability_subset" //MoltenVK
      #endif
  };
};

#endif /* Device_hpp */
