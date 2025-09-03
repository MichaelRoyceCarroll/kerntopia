#include "vulkan_memory.hpp"
#include "../common/logger.hpp"
#include "../common/error_handling.hpp"
#include <cstring>

// Vulkan headers conditionally included
#ifdef KERNTOPIA_VULKAN_SDK_AVAILABLE
#include <vulkan/vulkan.h>
#endif

namespace kerntopia {

// Vulkan internal structures (replicated from vulkan_runner.cpp)
struct VulkanDevice {
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice logical_device = VK_NULL_HANDLE;
    uint32_t compute_queue_family = 0;
    VkQueue compute_queue = VK_NULL_HANDLE;
    std::string device_name;
    DeviceInfo device_info;
};

// Helper to convert Vulkan error to string
std::string VulkanResultString(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
        default: return "VK_ERROR_UNKNOWN(" + std::to_string(result) + ")";
    }
}

// Helper function to find memory type for buffer allocation
uint32_t FindMemoryType(VkPhysicalDevice physical_device, uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
    
    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && 
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    // Return 0 as fallback - caller should check if allocation succeeds
    return 0;
}

// VulkanBuffer implementation
VulkanBuffer::VulkanBuffer(VulkanDevice* device, size_t size, Type type, Usage usage)
    : device_(device), size_(size), type_(type), usage_(usage) {
    CreateBuffer();
}

VulkanBuffer::~VulkanBuffer() {
    DestroyBuffer();
}

void* VulkanBuffer::Map() {
    if (is_mapped_) {
        return mapped_ptr_;
    }
    
    VkDeviceMemory vk_memory = static_cast<VkDeviceMemory>(device_memory_);
    if (!device_ || !device_->logical_device || vk_memory == VK_NULL_HANDLE) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "VulkanBuffer::Map - Invalid device or memory");
        return nullptr;
    }
    
    // Map the entire buffer memory using dynamically loaded function
    VkResult result = vkMapMemory(device_->logical_device, vk_memory, 0, size_, 0, &mapped_ptr_);
    if (result != VK_SUCCESS) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, 
            "VulkanBuffer::Map - Failed to map memory: " + VulkanResultString(result));
        return nullptr;
    }
    
    is_mapped_ = true;
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "VulkanBuffer mapped " + std::to_string(size_) + " bytes");
    return mapped_ptr_;
}

void VulkanBuffer::Unmap() {
    if (!is_mapped_) return;
    
    VkDeviceMemory vk_memory = static_cast<VkDeviceMemory>(device_memory_);
    if (!device_ || !device_->logical_device || vk_memory == VK_NULL_HANDLE) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "VulkanBuffer::Unmap - Invalid device or memory");
        return;
    }
    
    // Unmap the memory using dynamically loaded function
    vkUnmapMemory(device_->logical_device, vk_memory);
    mapped_ptr_ = nullptr;
    is_mapped_ = false;
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "VulkanBuffer unmapped");
}

Result<void> VulkanBuffer::UploadData(const void* data, size_t size, size_t offset) {
    if (offset + size > size_) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::VALIDATION, ErrorCode::INVALID_ARGUMENT,
                                     "Upload data exceeds buffer size");
    }
    
    // Simplified implementation - in real Vulkan would use staging buffer
    void* mapped = Map();
    if (!mapped) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::MEMORY_ALLOCATION_FAILED,
                                     "Failed to map buffer for upload");
    }
    
    std::memcpy(static_cast<uint8_t*>(mapped) + offset, data, size);
    Unmap();
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "VulkanBuffer uploaded " + std::to_string(size) + " bytes");
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> VulkanBuffer::DownloadData(void* data, size_t size, size_t offset) {
    if (offset + size > size_) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::VALIDATION, ErrorCode::INVALID_ARGUMENT,
                                     "Download data exceeds buffer size");
    }
    
    void* mapped = Map();
    if (!mapped) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::MEMORY_ALLOCATION_FAILED,
                                     "Failed to map buffer for download");
    }
    
    std::memcpy(data, static_cast<const uint8_t*>(mapped) + offset, size);
    Unmap();
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "VulkanBuffer downloaded " + std::to_string(size) + " bytes");
    return KERNTOPIA_VOID_SUCCESS();
}

bool VulkanBuffer::CreateBuffer() {
    if (!device_ || !device_->logical_device) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "VulkanBuffer::CreateBuffer - Invalid device");
        return false;
    }
    
    // Determine buffer usage flags based on buffer type and usage
    VkBufferUsageFlags usage_flags = 0;
    if (type_ == Type::STORAGE || usage_ == Usage::DYNAMIC) {
        usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    } else if (type_ == Type::UNIFORM) {
        usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    } else {
        usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; // Default to storage buffer
    }
    
    // Add transfer bits for host-device transfers
    usage_flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    
    // Create buffer using dynamically loaded function
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size_;
    buffer_info.usage = usage_flags;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkBuffer vk_buffer;
    VkResult result = vkCreateBuffer(device_->logical_device, &buffer_info, nullptr, &vk_buffer);
    if (result != VK_SUCCESS) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, 
            "VulkanBuffer::CreateBuffer - Failed to create buffer: " + VulkanResultString(result));
        return false;
    }
    
    // Store as void*
    buffer_ = static_cast<void*>(vk_buffer);
    
    // Get memory requirements using dynamically loaded function
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device_->logical_device, vk_buffer, &mem_requirements);
    
    // Determine memory properties - prefer host visible for CPU access
    VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    
    // Find suitable memory type
    uint32_t memory_type_index = FindMemoryType(device_->physical_device, 
                                               mem_requirements.memoryTypeBits, 
                                               memory_properties);
    
    // Allocate memory using dynamically loaded function
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = memory_type_index;
    
    VkDeviceMemory vk_memory;
    result = vkAllocateMemory(device_->logical_device, &alloc_info, nullptr, &vk_memory);
    if (result != VK_SUCCESS) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, 
            "VulkanBuffer::CreateBuffer - Failed to allocate memory: " + VulkanResultString(result));
        vkDestroyBuffer(device_->logical_device, vk_buffer, nullptr);
        buffer_ = nullptr;
        return false;
    }
    
    // Store as void*
    device_memory_ = static_cast<void*>(vk_memory);
    
    // Bind buffer to memory using dynamically loaded function
    result = vkBindBufferMemory(device_->logical_device, vk_buffer, vk_memory, 0);
    if (result != VK_SUCCESS) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, 
            "VulkanBuffer::CreateBuffer - Failed to bind buffer memory: " + VulkanResultString(result));
        vkFreeMemory(device_->logical_device, vk_memory, nullptr);
        vkDestroyBuffer(device_->logical_device, vk_buffer, nullptr);
        buffer_ = nullptr;
        device_memory_ = nullptr;
        return false;
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, 
        "VulkanBuffer created: " + std::to_string(size_) + " bytes, usage=0x" + 
        std::to_string(usage_flags) + ", memory_type=" + std::to_string(memory_type_index));
    return true;
}

void VulkanBuffer::DestroyBuffer() {
    // Safe to call multiple times - check if already destroyed
    if (buffer_ == nullptr && device_memory_ == nullptr) {
        return; // Already destroyed
    }
    
    if (!device_ || !device_->logical_device) {
        KERNTOPIA_LOG_WARNING(LogComponent::BACKEND, "VulkanBuffer::DestroyBuffer - Invalid device, clearing handles");
        // Device already destroyed, just clear our handles
        buffer_ = nullptr;
        device_memory_ = nullptr;
        mapped_ptr_ = nullptr;
        is_mapped_ = false;
        return;
    }
    
    // Unmap if currently mapped
    if (is_mapped_) {
        Unmap();
    }
    
    // Clean up Vulkan resources using dynamically loaded functions
    if (device_memory_ != nullptr) {
        VkDeviceMemory vk_memory = static_cast<VkDeviceMemory>(device_memory_);
        vkFreeMemory(device_->logical_device, vk_memory, nullptr);
        device_memory_ = nullptr;
    }
    
    if (buffer_ != nullptr) {
        VkBuffer vk_buffer = static_cast<VkBuffer>(buffer_);
        vkDestroyBuffer(device_->logical_device, vk_buffer, nullptr);
        buffer_ = nullptr;
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "VulkanBuffer destroyed");
}

// VulkanTexture implementation
VulkanTexture::VulkanTexture(VulkanDevice* device, const TextureDesc& desc)
    : device_(device), desc_(desc) {
    CreateImage();
}

VulkanTexture::~VulkanTexture() {
    DestroyImage();
}

Result<void> VulkanTexture::UploadData(const void* data, uint32_t mip_level, uint32_t array_layer) {
    // Simplified placeholder implementation
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "VulkanTexture upload data for mip " + std::to_string(mip_level));
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> VulkanTexture::DownloadData(void* data, size_t data_size, uint32_t mip_level, uint32_t array_layer) {
    // Simplified placeholder implementation
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "VulkanTexture download data for mip " + std::to_string(mip_level));
    return KERNTOPIA_VOID_SUCCESS();
}

bool VulkanTexture::CreateImage() {
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "VulkanTexture created " + 
                       std::to_string(desc_.width) + "x" + std::to_string(desc_.height));
    return true;
}

void VulkanTexture::DestroyImage() {
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "VulkanTexture destroyed");
}

uint32_t VulkanTexture::GetVulkanFormat() const {
    // VK_FORMAT_R8G8B8A8_UNORM = 37 - simplified mapping
    return 37;
}

} // namespace kerntopia