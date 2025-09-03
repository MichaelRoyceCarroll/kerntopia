#pragma once

#include "ikernel_runner.hpp"
#include <memory>
#include <string>

// Vulkan headers conditionally included
#ifdef KERNTOPIA_VULKAN_SDK_AVAILABLE
#include <vulkan/vulkan.h>
#endif

namespace kerntopia {

// Forward declarations
struct VulkanDevice;

// Helper functions used by Vulkan memory classes
#ifdef KERNTOPIA_VULKAN_SDK_AVAILABLE
std::string VulkanResultString(VkResult result);
uint32_t FindMemoryType(VkPhysicalDevice physical_device, uint32_t type_filter, VkMemoryPropertyFlags properties);

// External Vulkan function pointers - initialized by VulkanKernelRunner
extern PFN_vkCreateBuffer vkCreateBuffer;
extern PFN_vkDestroyBuffer vkDestroyBuffer;
extern PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
extern PFN_vkAllocateMemory vkAllocateMemory;
extern PFN_vkFreeMemory vkFreeMemory;
extern PFN_vkBindBufferMemory vkBindBufferMemory;
extern PFN_vkMapMemory vkMapMemory;
extern PFN_vkUnmapMemory vkUnmapMemory;
extern PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
#endif

/**
 * @brief Vulkan buffer implementation
 */
class VulkanBuffer : public IBuffer {
public:
    VulkanBuffer(VulkanDevice* device, size_t size, Type type, Usage usage);
    ~VulkanBuffer();
    
    size_t GetSize() const override { return size_; }
    Type GetType() const override { return type_; }
    
    void* Map() override;
    void Unmap() override;
    
    Result<void> UploadData(const void* data, size_t size, size_t offset = 0) override;
    Result<void> DownloadData(void* data, size_t size, size_t offset = 0) override;
    
    // Vulkan-specific methods  
    void* GetBuffer() const { return reinterpret_cast<void*>(buffer_); }
    void* GetDeviceMemory() const { return reinterpret_cast<void*>(device_memory_); }
    void DestroyBuffer();
    
private:
    VulkanDevice* device_;
    size_t size_;
    Type type_;
    Usage usage_;
    
    // Vulkan handles (stored as void* for header compatibility, cast to VkBuffer/VkDeviceMemory in implementation)
    void* buffer_ = nullptr;
    void* device_memory_ = nullptr;
    
    void* mapped_ptr_ = nullptr;
    bool is_mapped_ = false;
    
    bool CreateBuffer();
};

/**
 * @brief Vulkan texture implementation
 */
class VulkanTexture : public ITexture {
public:
    VulkanTexture(VulkanDevice* device, const TextureDesc& desc);
    ~VulkanTexture();
    
    const TextureDesc& GetDesc() const override { return desc_; }
    
    Result<void> UploadData(const void* data, uint32_t mip_level = 0, uint32_t array_layer = 0) override;
    Result<void> DownloadData(void* data, size_t data_size, uint32_t mip_level = 0, uint32_t array_layer = 0) override;
    
    // Vulkan-specific methods
    void* GetImage() const { return reinterpret_cast<void*>(image_); }
    void* GetImageView() const { return reinterpret_cast<void*>(image_view_); }
    void* GetDeviceMemory() const { return reinterpret_cast<void*>(device_memory_); }
    
private:
    VulkanDevice* device_;
    TextureDesc desc_;
    
    // Vulkan handles (stored as void* for header compatibility, cast to VkImage/VkImageView/VkDeviceMemory in implementation)
    void* image_ = nullptr;
    void* image_view_ = nullptr;
    void* device_memory_ = nullptr;
    
    bool CreateImage();
    void DestroyImage();
    uint32_t GetVulkanFormat() const;
};

} // namespace kerntopia