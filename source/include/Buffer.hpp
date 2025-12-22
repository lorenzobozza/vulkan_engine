//
//  Buffer.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 15/11/21.
//

#ifndef Buffer_hpp
#define Buffer_hpp

#include "Device.hpp"

class Buffer {
public:
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&) = delete;
    Buffer& operator=(Buffer&&) = delete;
    
    Buffer(const Device& device,
           VkDeviceSize instanceSize,
           uint32_t instanceCount,
           VkBufferUsageFlags usageFlags,
           VkMemoryPropertyFlags memoryPropertyFlags,
           VkDeviceSize minOffsetAlignment = 1);
    
    Buffer(const Device& device);
    ~Buffer();
    
    void destroy(void);
    VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void unmap(void);
    
    void createBuffer(VkDeviceSize bufferSize, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags);
    void writeToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const;
    VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    
    void writeToIndex(void* data, int index);
    VkResult flushIndex(int index);
    VkDescriptorBufferInfo descriptorInfoForIndex(int index);
    VkResult invalidateIndex(int index);
    
    VkBuffer getBuffer() const { return m_Buffer; }
    VkDeviceMemory getMemory() { return m_Memory; }
    void* getMappedMemory() const { return m_Mapped; }
    VkDeviceSize getBufferSize() const { return m_BufferSize; }
    uint32_t getInstanceCount() const { return m_InstanceCount; }
    VkDeviceSize getInstanceSize() const { return m_InstanceSize; }
    VkDeviceSize getAlignmentSize() const { return m_AlignmentSize; }
    VkBufferUsageFlags getUsageFlags() const { return m_UsageFlags; }
    VkMemoryPropertyFlags getMemoryPropertyFlags() const { return m_MemoryPropertyFlags; }
    
private:
    static VkDeviceSize getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);
    
    const Device& m_Device;
    
    VkBuffer m_Buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory = VK_NULL_HANDLE;
    void* m_Mapped = nullptr;
    
    VkDeviceSize m_BufferSize;
    uint32_t m_InstanceCount;
    VkDeviceSize m_InstanceSize;
    VkDeviceSize m_AlignmentSize;
    VkBufferUsageFlags m_UsageFlags;
    VkMemoryPropertyFlags m_MemoryPropertyFlags;
};

#endif /* Buffer_hpp */
