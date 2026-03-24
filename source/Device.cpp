//
//  Device.cpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 05/11/21.
//

#include "include/Device.hpp"
#include "Log.hpp"

#include <set>
#include <unordered_set>
#include <stdexcept>

// local callback functions
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
    if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        //std::println("~Validation Layer~\n{}", pCallbackData->pMessage);
        Log::getInstance()->error("~Validation Layer~\n{}", pCallbackData->pMessage);
    }
    return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

Device::Device(SDLWindow &window) : m_Window(window) {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createGraphicsCommandPool();
    createTransferCommandPool();
    
    ext = std::make_unique<Extensions>(this);
}

Device::~Device() {
    vkDestroyCommandPool(m_DeviceHandle, m_TransferCommandPool, nullptr);
    vkDestroyCommandPool(m_DeviceHandle, m_GraphicsCommandPool, nullptr);
    vkDestroyDevice(m_DeviceHandle, nullptr);
    
    if (ValidationLayersEnabled) {
        DestroyDebugUtilsMessengerEXT(m_InstanceHandle, m_DebugMessenger, nullptr);
    }
    
    vkDestroySurfaceKHR(m_InstanceHandle, m_SurfaceHandle, nullptr);
    vkDestroyInstance(m_InstanceHandle, nullptr);
}

void Device::createInstance(void) {
    if (ValidationLayersEnabled) {
        if(!checkValidationLayerSupport())
            throw std::runtime_error("Validation layers requested, but not available!");
    }
    
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VulkanEngine App";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 2, 0);
    appInfo.pEngineName = "Test Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 2, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;
    
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    
    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (ValidationLayersEnabled) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_RequiredLayers.size());
        createInfo.ppEnabledLayerNames = m_RequiredLayers.data();
        
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }
    
    if (vkCreateInstance(&createInfo, nullptr, &m_InstanceHandle) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create instance!");
    }
    
    hasRequiredInstanceExtensions();
}

VkSampleCountFlagBits Device::getMaxUsableSampleCount(void) {
    VkSampleCountFlags counts = m_DeviceProperties.limits.framebufferColorSampleCounts & m_DeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }
    
    return VK_SAMPLE_COUNT_1_BIT;
}

void Device::pickPhysicalDevice(void) {
    Log* log = Log::getInstance();
    
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_InstanceHandle, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find Devices with Vulkan support!");
    }
    log->info("Device count: {}", deviceCount);
    
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_InstanceHandle, &deviceCount, devices.data());
    for (auto& device : devices) {
        if (isDeviceSuitable(device)) {
            m_PhysicalDevice = device;
            break;
        }
    }
    
    if (m_PhysicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable Device!");
    }
    
    VkPhysicalDeviceDriverProperties driverProp {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES
    };
    VkPhysicalDeviceProperties2 deviceProp {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = (void*)&driverProp
    };
    vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &deviceProp);
    m_DeviceProperties = deviceProp.properties;
    m_MaxMSAASamples = getMaxUsableSampleCount();
    m_MemoryUMA = (m_DeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
    
    log->info("Vulkan {}.{}.{} - {} - {}",
              VK_API_VERSION_MAJOR(m_DeviceProperties.apiVersion),
              VK_API_VERSION_MINOR(m_DeviceProperties.apiVersion),
              VK_API_VERSION_PATCH(m_DeviceProperties.apiVersion),
              std::string(driverProp.driverName), std::string(m_DeviceProperties.deviceName));
}

void Device::createLogicalDevice(void) {
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    
    struct QueueFamily {
        uint32_t id;
        uint32_t count;
    };
    std::vector<QueueFamily> uniqueQueueFamilies;
    if (m_Indices.graphicsFamily != m_Indices.transferFamily) {
        uniqueQueueFamilies.push_back({m_Indices.graphicsFamily, 1U});
        uniqueQueueFamilies.push_back({m_Indices.transferFamily, 1U});
    } else {
        uniqueQueueFamilies.push_back({m_Indices.graphicsFamily, (m_Indices.enableAsyncTransfer ? 2U : 1U)});
    }
    
    float queuePriority[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    for (auto& queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily.id;
        queueCreateInfo.queueCount = queueFamily.count;
        queueCreateInfo.pQueuePriorities = queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }
    
    VkPhysicalDeviceFeatures vulkan10supportedFeatures = {};
    VkPhysicalDeviceFeatures vulkan10requestedFeatures = {};
    
    VkPhysicalDeviceVulkan12Features vulkan12supportedFeatures = {};
    vulkan12supportedFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12supportedFeatures.pNext = nullptr;
    
    VkPhysicalDeviceVulkan12Features vulkan12requestedFeatures = {};
    vulkan12requestedFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12requestedFeatures.pNext = nullptr;
    
    VkPhysicalDeviceFeatures2 supportedFeatures2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &vulkan12supportedFeatures
    };
    
    vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &vulkan10supportedFeatures);
    vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &supportedFeatures2);
    
    vulkan10requestedFeatures.samplerAnisotropy = vulkan10supportedFeatures.samplerAnisotropy;
    vulkan10requestedFeatures.sampleRateShading = vulkan10supportedFeatures.sampleRateShading;
    vulkan10requestedFeatures.fillModeNonSolid = vulkan10supportedFeatures.fillModeNonSolid;
    vulkan10requestedFeatures.shaderSampledImageArrayDynamicIndexing = vulkan10supportedFeatures.shaderSampledImageArrayDynamicIndexing;
    
    vulkan12requestedFeatures.descriptorBindingPartiallyBound = vulkan12supportedFeatures.descriptorBindingPartiallyBound;
    
    VkPhysicalDeviceFeatures2 requestedFeatures2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .features = vulkan10requestedFeatures,
        .pNext = &vulkan12requestedFeatures
    };
        
    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = nullptr;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_RequiredExtensions.size());
    createInfo.ppEnabledExtensionNames = m_RequiredExtensions.data();
    createInfo.pNext = &requestedFeatures2;
    
    // might not really be necessary anymore because device specific validation layers have been deprecated
    if (ValidationLayersEnabled) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_RequiredLayers.size());
        createInfo.ppEnabledLayerNames = m_RequiredLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }
    
    if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_DeviceHandle) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device!");
    }
    
    vkGetDeviceQueue(m_DeviceHandle, m_Indices.graphicsFamily, m_Indices.graphicsQueueCount, &m_GraphicsQueue);
    vkGetDeviceQueue(m_DeviceHandle, m_Indices.transferFamily, m_Indices.transferQueueCount, &m_TransferQueue);
    vkGetDeviceQueue(m_DeviceHandle, m_Indices.presentFamily, 0, &m_PresentQueue);
}

void Device::createGraphicsCommandPool(void) {
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_Indices.graphicsFamily;
    poolInfo.flags =
    VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    
    if (vkCreateCommandPool(m_DeviceHandle, &poolInfo, nullptr, &m_GraphicsCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics command pool!");
    }
}

void Device::createTransferCommandPool(void) {
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_Indices.transferFamily;
    poolInfo.flags =
    VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    
    if (vkCreateCommandPool(m_DeviceHandle, &poolInfo, nullptr, &m_TransferCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create transfer command pool!");
    }
}

void Device::createSurface(void) { m_Window.createWindowSurface(m_InstanceHandle, &m_SurfaceHandle); }

bool Device::isDeviceSuitable(VkPhysicalDevice device) {
    m_Indices = findQueueFamilies(device);
        
    bool swapChainAdequate = false;
    bool extensionsSupported = checkDeviceExtensionSupport(device);
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }
    
    return m_Indices.isComplete() && extensionsSupported && swapChainAdequate;
}

void Device::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;
}

void Device::setupDebugMessenger(void) {
    if (!ValidationLayersEnabled) return;
    
    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);
    if (CreateDebugUtilsMessengerEXT(m_InstanceHandle, &createInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("Failed to set up debug messenger!");
    }
}

bool Device::checkValidationLayerSupport(void) {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    
    for (const char *layerName : m_RequiredLayers) {
        bool layerFound = false;
        
        for (const auto &layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        
        if (!layerFound) {
            return false;
        }
    }
    
    return true;
}

// TODO: Make a nice version
std::vector<const char *> Device::getRequiredExtensions(void) {
    uint32_t sdlExtensionCount;
    const char* const* requred_ext = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
    if (requred_ext == NULL) {
        throw std::runtime_error("Failed to get SDL extensions!");
    }
    
    std::vector<const char*> extensions = {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
    };
    size_t addedExtensionCount = extensions.size();
    extensions.resize(addedExtensionCount + sdlExtensionCount);
    
    for (uint32_t i = 0; i < sdlExtensionCount; i++) {
        extensions.at(addedExtensionCount + i) = requred_ext[i];
    }
    
    return extensions;
}

void Device::hasRequiredInstanceExtensions(void) {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
    
    std::unordered_set<std::string> available;
    for (const auto &extension : extensions) {
        available.insert(extension.extensionName);
    }
    
    auto requiredExtensions = getRequiredExtensions();
    for (const auto &required : requiredExtensions) {
        if (available.find(required) == available.end()) {
            throw std::runtime_error("Missing required SDL3 extension");
        }
    }
}

bool Device::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    
    std::set<std::string> requiredExtensionsChecklist(m_RequiredExtensions.begin(), m_RequiredExtensions.end());
    
    for (const auto& ext : availableExtensions) {
        size_t required = requiredExtensionsChecklist.erase(ext.extensionName);
        if ((std::string(ext.extensionName) == "VK_KHR_portability_subset") && !required) return false;
    }
    
    return requiredExtensionsChecklist.empty();
}

QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;
    
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
    
    
    // Priority to find a family that supports both graphics and present
    for (int i = 0; i < queueFamilies.size(); ++i) {
        VkBool32 present = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_SurfaceHandle, &present);
        auto& family = queueFamilies[i];
        if (family.queueCount > 0 && present && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            indices.presentFamily = i;
            indices.presentFamilyHasValue = true;
            indices.presentQueueCount = 0;
            indices.graphicsFamily = i;
            indices.graphicsQueueCount = 0;
            indices.graphicsFamilyHasValue = true;
            break;
        }
    }
    
    if (!indices.presentFamilyHasValue) {
        for (int i = 0; i < queueFamilies.size(); ++i) {
            VkBool32 present = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_SurfaceHandle, &present);
            if (present) {
                indices.presentFamily = i;
                indices.presentFamilyHasValue = true;
                break;
            }
        }
    }
    
    if (!indices.graphicsFamilyHasValue) {
        for (int i = 0; i < queueFamilies.size(); ++i) {
            auto& family = queueFamilies[i];
            if (family.queueCount > 0 && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                indices.graphicsFamily = i;
                indices.graphicsQueueCount = 0;
                indices.graphicsFamilyHasValue = true;
                break;
            }
        }
    }
    
    for (int i = 0; i < queueFamilies.size(); ++i) {
        auto& family = queueFamilies[i];
        if (family.queueCount > 0 && (family.queueFlags & VK_QUEUE_TRANSFER_BIT)) {
            if (indices.graphicsFamily == i && family.queueCount == 1) continue;
            indices.transferFamily = i;
            indices.transferQueueCount = (indices.graphicsFamily == i) ? 1 : 0;
            indices.transferFamilyHasValue = true;
            break;
        }
    }
    
    // No async transfer
    if (!indices.transferFamilyHasValue) {
        indices.transferFamily = indices.graphicsFamily;
        indices.transferQueueCount = 0;
        indices.transferFamilyHasValue = true;
    } else {
        indices.enableAsyncTransfer = true;
    }
    
    return indices;
}

SwapChainSupportDetails Device::querySwapChainSupport(VkPhysicalDevice device) const {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_SurfaceHandle, &details.capabilities);
    
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_SurfaceHandle, &formatCount, nullptr);
    
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_SurfaceHandle, &formatCount, details.formats.data());
    }
    
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_SurfaceHandle, &presentModeCount, nullptr);
    
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_SurfaceHandle, &presentModeCount, details.presentModes.data());
    }
    return details;
}

VkFormat Device::findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &props);
        
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("Failed to find supported format!");
}

uint32_t Device::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    throw std::runtime_error("Failed to find suitable memory type!");
}

void Device::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags prop, VkBuffer &buffer, VkDeviceMemory &bufferMemory) const {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(m_DeviceHandle, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vertex buffer!");
    }
    
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_DeviceHandle, buffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, prop);
    
    if (vkAllocateMemory(m_DeviceHandle, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate vertex buffer memory!");
    }
    
    vkBindBufferMemory(m_DeviceHandle, buffer, bufferMemory, 0);
}

VkCommandBuffer Device::beginSingleTimeCommands(void) const {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_TransferCommandPool;
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_DeviceHandle, &allocInfo, &commandBuffer);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void Device::endSingleTimeCommands(VkCommandBuffer commandBuffer) const {
    vkEndCommandBuffer(commandBuffer);
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    vkQueueSubmit(m_TransferQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_TransferQueue);
    
    vkFreeCommandBuffers(m_DeviceHandle, m_TransferCommandPool, 1, &commandBuffer);
}

void Device::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    
    endSingleTimeCommands(commandBuffer);
}

void Device::createImageWithInfo(const VkImageCreateInfo &imageInfo, VkMemoryPropertyFlags properties,
                                 VkImage& image, VkDeviceMemory &imageMemory) const {
    if (vkCreateImage(m_DeviceHandle, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image!");
    }
    
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_DeviceHandle, image, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
    
    if (vkAllocateMemory(m_DeviceHandle, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory!");
    }
    
    if (vkBindImageMemory(m_DeviceHandle, image, imageMemory, 0) != VK_SUCCESS) {
        throw std::runtime_error("Failed to bind image memory!");
    }
}
