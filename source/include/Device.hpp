//
//  Device.hpp
//  vulkan_engine
//
//  Created by Brendan Galea on 05/11/21.
//

#ifndef Device_hpp
#define Device_hpp

#include "SDLWindow.hpp"

#include <vector>
#include <memory>

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

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
    bool enableAsyncTransfer = false;
    bool isComplete() { return graphicsFamilyHasValue && transferFamilyHasValue && presentFamilyHasValue; }
};

class Device {
public:
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;
    
    Device(SDLWindow &window);
    ~Device();
    
    class Extensions {
    public:
        Extensions(const Extensions&) = delete;
        Extensions& operator=(const Extensions&) = delete;
        Extensions(Extensions&&) = delete;
        Extensions& operator=(Extensions&&) = delete;
        
        Extensions(Device* device) : m_Device(device) {
            m_PfnSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(m_Device->device(), "vkSetDebugUtilsObjectNameEXT");
        }
        ~Extensions() = default;
        
        void setDebugUtilsObjectName(VkObjectType type, uint64_t object, std::string name)
        {
#ifdef DEBUG
            VkDebugUtilsObjectNameInfoEXT nameInfo {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .objectType = type,
                .objectHandle = object,
                .pObjectName = name.c_str()
            };
            m_PfnSetDebugUtilsObjectNameEXT(m_Device->device(), &nameInfo);
#endif
        }
        
    private:
        Device* m_Device;
        PFN_vkSetDebugUtilsObjectNameEXT m_PfnSetDebugUtilsObjectNameEXT;
    };
    
#ifdef DEBUG
    constexpr static bool ValidationLayersEnabled = true;
#else
    constexpr static bool ValidationLayersEnabled = false;
#endif
    
    VkSurfaceKHR surface(void) const { return m_SurfaceHandle; }
    VkPhysicalDevice getPhysicalDevice(void) const { return m_PhysicalDevice; }
    VkDevice device(void) const { return m_DeviceHandle; }
    VkQueue graphicsQueue(void) const { return m_GraphicsQueue; }
    VkQueue transferQueue(void) const { return m_TransferQueue; }
    VkQueue presentQueue(void) const { return m_PresentQueue; }
    VkCommandPool getCommandPool(void) const { return m_GraphicsCommandPool; }
    VkCommandPool getTransferCommandPool(void) const { return m_TransferCommandPool; }
    QueueFamilyIndices getFamilyIndices(void) const { return m_Indices; }
    VkPhysicalDeviceProperties getPhysicalDeviceProp(void) const { return m_DeviceProperties; }
    VkSampleCountFlagBits getSupportedSmapleCount(void) const { return m_MaxMSAASamples; }

    SwapChainSupportDetails getSwapChainSupport() const { return querySwapChainSupport(m_PhysicalDevice); }
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
    
    VkCommandBuffer beginSingleTimeCommands(void) const;
    void endSingleTimeCommands(VkCommandBuffer commandBuffer) const;
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const;
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags prop, VkBuffer& buffer, VkDeviceMemory& bufferMemory) const;
    void createImageWithInfo(const VkImageCreateInfo& imageInfo, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory &imageMemory) const;
    
    std::unique_ptr<Extensions> ext;

private:
    void createInstance(void);
    void setupDebugMessenger(void);
    void createSurface(void);
    void pickPhysicalDevice(void);
    void createLogicalDevice(void);
    void createGraphicsCommandPool(void);
    void createTransferCommandPool(void);
    
    void hasRequiredInstanceExtensions(void);
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkValidationLayerSupport(void);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    std::vector<const char *> getRequiredExtensions(void);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    VkSampleCountFlagBits getMaxUsableSampleCount(void);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);
    
    SDLWindow& m_Window;
    VkInstance m_InstanceHandle = VK_NULL_HANDLE;
    VkSurfaceKHR m_SurfaceHandle = VK_NULL_HANDLE;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_DeviceHandle = VK_NULL_HANDLE;
    VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
    VkQueue m_TransferQueue = VK_NULL_HANDLE;
    VkQueue m_PresentQueue = VK_NULL_HANDLE;
    VkCommandPool m_GraphicsCommandPool = VK_NULL_HANDLE;
    VkCommandPool m_TransferCommandPool = VK_NULL_HANDLE;
    QueueFamilyIndices m_Indices;
    
    VkPhysicalDeviceProperties m_DeviceProperties;
    VkDebugUtilsMessengerEXT m_DebugMessenger;
    VkSampleCountFlagBits m_MaxMSAASamples;

    const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
    const std::vector<const char *> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef __APPLE__
        "VK_KHR_portability_subset"
#endif
    };
};

#endif /* Device_hpp */
