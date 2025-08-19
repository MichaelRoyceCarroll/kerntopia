#include "vulkan_runner.hpp"
#include "../common/logger.hpp"
#include "runtime_loader.hpp"
#include "../system/system_interrogator.hpp"
#include "../system/interrogation_data.hpp"

#include <cstring>
#include <vector>
#include <sstream>
#include <thread>
#include <iostream>

// Include official Vulkan headers
#ifdef KERNTOPIA_VULKAN_SDK_AVAILABLE
#include <vulkan/vulkan.h>
#else
#error "Vulkan SDK not available - VULKAN_SDK environment variable must be set"
#endif

namespace kerntopia {

// Vulkan function pointers (dynamically loaded)
// Note: Using official Vulkan types from vulkan.h instead of manual definitions

// Vulkan function type definitions using official PFN_ types

// Instance functions
typedef PFN_vkCreateInstance vkCreateInstance_t;
typedef PFN_vkDestroyInstance vkDestroyInstance_t;
typedef PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices_t;
typedef PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties_t;
typedef PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties_t;
typedef PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures_t;
typedef PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties_t;

// Device functions
typedef PFN_vkCreateDevice vkCreateDevice_t;
typedef PFN_vkDestroyDevice vkDestroyDevice_t;
typedef PFN_vkGetDeviceQueue vkGetDeviceQueue_t;

// Buffer functions
typedef PFN_vkCreateBuffer vkCreateBuffer_t;
typedef PFN_vkDestroyBuffer vkDestroyBuffer_t;
typedef PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements_t;

// Memory functions
typedef PFN_vkAllocateMemory vkAllocateMemory_t;
typedef PFN_vkFreeMemory vkFreeMemory_t;
typedef PFN_vkBindBufferMemory vkBindBufferMemory_t;
typedef PFN_vkMapMemory vkMapMemory_t;
typedef PFN_vkUnmapMemory vkUnmapMemory_t;

// Shader and pipeline functions
typedef PFN_vkCreateShaderModule vkCreateShaderModule_t;
typedef PFN_vkDestroyShaderModule vkDestroyShaderModule_t;
typedef PFN_vkCreatePipelineLayout vkCreatePipelineLayout_t;
typedef PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout_t;
typedef PFN_vkCreateComputePipelines vkCreateComputePipelines_t;
typedef PFN_vkDestroyPipeline vkDestroyPipeline_t;

// Descriptor set functions
typedef PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout_t;
typedef PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout_t;
typedef PFN_vkCreateDescriptorPool vkCreateDescriptorPool_t;
typedef PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool_t;
typedef PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets_t;
typedef PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets_t;

// Command buffer functions
typedef PFN_vkCreateCommandPool vkCreateCommandPool_t;
typedef PFN_vkDestroyCommandPool vkDestroyCommandPool_t;
typedef PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers_t;
typedef PFN_vkFreeCommandBuffers vkFreeCommandBuffers_t;
typedef PFN_vkBeginCommandBuffer vkBeginCommandBuffer_t;
typedef PFN_vkEndCommandBuffer vkEndCommandBuffer_t;
typedef PFN_vkCmdBindPipeline vkCmdBindPipeline_t;
typedef PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets_t;
typedef PFN_vkCmdDispatch vkCmdDispatch_t;

// Queue and synchronization functions
typedef PFN_vkQueueSubmit vkQueueSubmit_t;
typedef PFN_vkQueueWaitIdle vkQueueWaitIdle_t;
typedef PFN_vkCreateFence vkCreateFence_t;
typedef PFN_vkDestroyFence vkDestroyFence_t;
typedef PFN_vkWaitForFences vkWaitForFences_t;
typedef PFN_vkResetFences vkResetFences_t;

// Static Vulkan runtime functions (loaded dynamically)

// Instance functions
static vkCreateInstance_t vkCreateInstance = nullptr;
static vkDestroyInstance_t vkDestroyInstance = nullptr;
static vkEnumeratePhysicalDevices_t vkEnumeratePhysicalDevices = nullptr;
static vkGetPhysicalDeviceProperties_t vkGetPhysicalDeviceProperties = nullptr;
static vkGetPhysicalDeviceMemoryProperties_t vkGetPhysicalDeviceMemoryProperties = nullptr;
static vkGetPhysicalDeviceFeatures_t vkGetPhysicalDeviceFeatures = nullptr;
static vkGetPhysicalDeviceQueueFamilyProperties_t vkGetPhysicalDeviceQueueFamilyProperties = nullptr;

// Device functions
static vkCreateDevice_t vkCreateDevice = nullptr;
static vkDestroyDevice_t vkDestroyDevice = nullptr;
static vkGetDeviceQueue_t vkGetDeviceQueue = nullptr;

// Buffer functions
static vkCreateBuffer_t vkCreateBuffer = nullptr;
static vkDestroyBuffer_t vkDestroyBuffer = nullptr;
static vkGetBufferMemoryRequirements_t vkGetBufferMemoryRequirements = nullptr;

// Memory functions
static vkAllocateMemory_t vkAllocateMemory = nullptr;
static vkFreeMemory_t vkFreeMemory = nullptr;
static vkBindBufferMemory_t vkBindBufferMemory = nullptr;
static vkMapMemory_t vkMapMemory = nullptr;
static vkUnmapMemory_t vkUnmapMemory = nullptr;

// Shader and pipeline functions
static vkCreateShaderModule_t vkCreateShaderModule = nullptr;
static vkDestroyShaderModule_t vkDestroyShaderModule = nullptr;
static vkCreatePipelineLayout_t vkCreatePipelineLayout = nullptr;
static vkDestroyPipelineLayout_t vkDestroyPipelineLayout = nullptr;
static vkCreateComputePipelines_t vkCreateComputePipelines = nullptr;
static vkDestroyPipeline_t vkDestroyPipeline = nullptr;

// Descriptor set functions
static vkCreateDescriptorSetLayout_t vkCreateDescriptorSetLayout = nullptr;
static vkDestroyDescriptorSetLayout_t vkDestroyDescriptorSetLayout = nullptr;
static vkCreateDescriptorPool_t vkCreateDescriptorPool = nullptr;
static vkDestroyDescriptorPool_t vkDestroyDescriptorPool = nullptr;
static vkAllocateDescriptorSets_t vkAllocateDescriptorSets = nullptr;
static vkUpdateDescriptorSets_t vkUpdateDescriptorSets = nullptr;

// Command buffer functions
static vkCreateCommandPool_t vkCreateCommandPool = nullptr;
static vkDestroyCommandPool_t vkDestroyCommandPool = nullptr;
static vkAllocateCommandBuffers_t vkAllocateCommandBuffers = nullptr;
static vkFreeCommandBuffers_t vkFreeCommandBuffers = nullptr;
static vkBeginCommandBuffer_t vkBeginCommandBuffer = nullptr;
static vkEndCommandBuffer_t vkEndCommandBuffer = nullptr;
static vkCmdBindPipeline_t vkCmdBindPipeline = nullptr;
static vkCmdBindDescriptorSets_t vkCmdBindDescriptorSets = nullptr;
static vkCmdDispatch_t vkCmdDispatch = nullptr;

// Queue and synchronization functions
static vkQueueSubmit_t vkQueueSubmit = nullptr;
static vkQueueWaitIdle_t vkQueueWaitIdle = nullptr;
static vkCreateFence_t vkCreateFence = nullptr;
static vkDestroyFence_t vkDestroyFence = nullptr;
static vkWaitForFences_t vkWaitForFences = nullptr;
static vkResetFences_t vkResetFences = nullptr;

static LibraryHandle vulkan_loader_handle = nullptr;

// Helper function to load Vulkan loader
static Result<void> LoadVulkanLoader() {
    if (vulkan_loader_handle) {
        return KERNTOPIA_VOID_SUCCESS(); // Already loaded
    }
    
    RuntimeLoader loader;
    
    // Use same search strategy as detection: respect LD_LIBRARY_PATH first, then system paths
    std::vector<std::string> vulkan_candidates;
    std::string selected_loader;
    
    // First priority: Use ScanForLibraries (respects LD_LIBRARY_PATH)
    std::vector<std::string> vulkan_patterns = {"vulkan", "vulkan-1"};
    auto scan_result = loader.ScanForLibraries(vulkan_patterns);
    if (scan_result) {
        for (const auto& [name, lib_info] : *scan_result) {
            vulkan_candidates.push_back(lib_info.full_path);
        }
    }
    
    // Second priority: Add standard system paths as fallbacks
    std::vector<std::string> system_paths = {
        "/usr/lib/x86_64-linux-gnu/libvulkan.so.1",
        "/usr/lib/x86_64-linux-gnu/libvulkan.so",
        "/usr/lib/libvulkan.so.1", 
        "/usr/lib/libvulkan.so"
    };
    
    // Add system paths that weren't already found by scan
    for (const std::string& path : system_paths) {
        if (std::find(vulkan_candidates.begin(), vulkan_candidates.end(), path) == vulkan_candidates.end()) {
            vulkan_candidates.push_back(path);
        }
    }
    
    // Log all candidates found
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Vulkan loader candidates found: " + std::to_string(vulkan_candidates.size()));
    for (size_t i = 0; i < vulkan_candidates.size(); ++i) {
        KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "  [" + std::to_string(i+1) + "] " + vulkan_candidates[i]);
    }
    
    // Try to load first successfully loadable candidate (respects search priority)
    for (const std::string& candidate : vulkan_candidates) {
        auto load_result = loader.LoadLibrary(candidate);
        if (load_result) {
            vulkan_loader_handle = *load_result;
            selected_loader = candidate;
            KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Selected Vulkan loader: " + candidate);
            break;
        } else {
            KERNTOPIA_LOG_WARNING(LogComponent::BACKEND, "Failed to load candidate: " + candidate);
        }
    }
    
    if (!vulkan_loader_handle) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to load Vulkan loader library");
    }
    
    // Load all required function pointers for compute pipeline support
    
    // Instance functions
    vkCreateInstance = reinterpret_cast<vkCreateInstance_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkCreateInstance"));
    vkDestroyInstance = reinterpret_cast<vkDestroyInstance_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkDestroyInstance"));
    vkEnumeratePhysicalDevices = reinterpret_cast<vkEnumeratePhysicalDevices_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkEnumeratePhysicalDevices"));
    vkGetPhysicalDeviceProperties = reinterpret_cast<vkGetPhysicalDeviceProperties_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkGetPhysicalDeviceProperties"));
    vkGetPhysicalDeviceMemoryProperties = reinterpret_cast<vkGetPhysicalDeviceMemoryProperties_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkGetPhysicalDeviceMemoryProperties"));
    vkGetPhysicalDeviceFeatures = reinterpret_cast<vkGetPhysicalDeviceFeatures_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkGetPhysicalDeviceFeatures"));
    vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<vkGetPhysicalDeviceQueueFamilyProperties_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkGetPhysicalDeviceQueueFamilyProperties"));
    
    // Device functions
    vkCreateDevice = reinterpret_cast<vkCreateDevice_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkCreateDevice"));
    vkDestroyDevice = reinterpret_cast<vkDestroyDevice_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkDestroyDevice"));
    vkGetDeviceQueue = reinterpret_cast<vkGetDeviceQueue_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkGetDeviceQueue"));
    
    // Buffer functions
    vkCreateBuffer = reinterpret_cast<vkCreateBuffer_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkCreateBuffer"));
    vkDestroyBuffer = reinterpret_cast<vkDestroyBuffer_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkDestroyBuffer"));
    vkGetBufferMemoryRequirements = reinterpret_cast<vkGetBufferMemoryRequirements_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkGetBufferMemoryRequirements"));
    
    // Memory functions
    vkAllocateMemory = reinterpret_cast<vkAllocateMemory_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkAllocateMemory"));
    vkFreeMemory = reinterpret_cast<vkFreeMemory_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkFreeMemory"));
    vkBindBufferMemory = reinterpret_cast<vkBindBufferMemory_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkBindBufferMemory"));
    vkMapMemory = reinterpret_cast<vkMapMemory_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkMapMemory"));
    vkUnmapMemory = reinterpret_cast<vkUnmapMemory_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkUnmapMemory"));
    
    // Shader and pipeline functions
    vkCreateShaderModule = reinterpret_cast<vkCreateShaderModule_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkCreateShaderModule"));
    vkDestroyShaderModule = reinterpret_cast<vkDestroyShaderModule_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkDestroyShaderModule"));
    vkCreatePipelineLayout = reinterpret_cast<vkCreatePipelineLayout_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkCreatePipelineLayout"));
    vkDestroyPipelineLayout = reinterpret_cast<vkDestroyPipelineLayout_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkDestroyPipelineLayout"));
    vkCreateComputePipelines = reinterpret_cast<vkCreateComputePipelines_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkCreateComputePipelines"));
    vkDestroyPipeline = reinterpret_cast<vkDestroyPipeline_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkDestroyPipeline"));
    
    // Descriptor set functions
    vkCreateDescriptorSetLayout = reinterpret_cast<vkCreateDescriptorSetLayout_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkCreateDescriptorSetLayout"));
    vkDestroyDescriptorSetLayout = reinterpret_cast<vkDestroyDescriptorSetLayout_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkDestroyDescriptorSetLayout"));
    vkCreateDescriptorPool = reinterpret_cast<vkCreateDescriptorPool_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkCreateDescriptorPool"));
    vkDestroyDescriptorPool = reinterpret_cast<vkDestroyDescriptorPool_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkDestroyDescriptorPool"));
    vkAllocateDescriptorSets = reinterpret_cast<vkAllocateDescriptorSets_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkAllocateDescriptorSets"));
    vkUpdateDescriptorSets = reinterpret_cast<vkUpdateDescriptorSets_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkUpdateDescriptorSets"));
    
    // Command buffer functions
    vkCreateCommandPool = reinterpret_cast<vkCreateCommandPool_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkCreateCommandPool"));
    vkDestroyCommandPool = reinterpret_cast<vkDestroyCommandPool_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkDestroyCommandPool"));
    vkAllocateCommandBuffers = reinterpret_cast<vkAllocateCommandBuffers_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkAllocateCommandBuffers"));
    vkFreeCommandBuffers = reinterpret_cast<vkFreeCommandBuffers_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkFreeCommandBuffers"));
    vkBeginCommandBuffer = reinterpret_cast<vkBeginCommandBuffer_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkBeginCommandBuffer"));
    vkEndCommandBuffer = reinterpret_cast<vkEndCommandBuffer_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkEndCommandBuffer"));
    vkCmdBindPipeline = reinterpret_cast<vkCmdBindPipeline_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkCmdBindPipeline"));
    vkCmdBindDescriptorSets = reinterpret_cast<vkCmdBindDescriptorSets_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkCmdBindDescriptorSets"));
    vkCmdDispatch = reinterpret_cast<vkCmdDispatch_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkCmdDispatch"));
    
    // Queue and synchronization functions
    vkQueueSubmit = reinterpret_cast<vkQueueSubmit_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkQueueSubmit"));
    vkQueueWaitIdle = reinterpret_cast<vkQueueWaitIdle_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkQueueWaitIdle"));
    vkCreateFence = reinterpret_cast<vkCreateFence_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkCreateFence"));
    vkDestroyFence = reinterpret_cast<vkDestroyFence_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkDestroyFence"));
    vkWaitForFences = reinterpret_cast<vkWaitForFences_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkWaitForFences"));
    vkResetFences = reinterpret_cast<vkResetFences_t>(
        loader.GetSymbol(vulkan_loader_handle, "vkResetFences"));
    
    // Verify all critical functions were loaded for compute pipeline support
    if (!vkCreateInstance || !vkDestroyInstance || !vkEnumeratePhysicalDevices || 
        !vkGetPhysicalDeviceProperties || !vkGetPhysicalDeviceMemoryProperties || !vkGetPhysicalDeviceFeatures ||
        !vkGetPhysicalDeviceQueueFamilyProperties || !vkCreateDevice || !vkDestroyDevice || !vkGetDeviceQueue ||
        !vkCreateBuffer || !vkDestroyBuffer || !vkGetBufferMemoryRequirements || 
        !vkAllocateMemory || !vkFreeMemory || !vkBindBufferMemory || !vkMapMemory || !vkUnmapMemory ||
        !vkCreateShaderModule || !vkDestroyShaderModule || !vkCreatePipelineLayout || !vkDestroyPipelineLayout ||
        !vkCreateComputePipelines || !vkDestroyPipeline ||
        !vkCreateDescriptorSetLayout || !vkDestroyDescriptorSetLayout || !vkCreateDescriptorPool || !vkDestroyDescriptorPool ||
        !vkAllocateDescriptorSets || !vkUpdateDescriptorSets ||
        !vkCreateCommandPool || !vkDestroyCommandPool || !vkAllocateCommandBuffers || !vkFreeCommandBuffers ||
        !vkBeginCommandBuffer || !vkEndCommandBuffer || !vkCmdBindPipeline || !vkCmdBindDescriptorSets || !vkCmdDispatch ||
        !vkQueueSubmit || !vkQueueWaitIdle || !vkCreateFence || !vkDestroyFence || !vkWaitForFences || !vkResetFences) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to load required Vulkan compute pipeline functions");
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

// Helper to convert Vulkan error to string
static std::string VulkanResultString(VkResult result) {
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

// Helper to convert device type to string
static std::string DeviceTypeString(uint32_t deviceType) {
    switch (deviceType) {
        case 0: return "Other";
        case 1: return "Integrated GPU";
        case 2: return "Discrete GPU";
        case 3: return "Virtual GPU";
        case 4: return "CPU";
        default: return "Unknown";
    }
}

// Vulkan internal structures using official Vulkan handle types
struct VulkanContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
};

struct VulkanDevice {
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice logical_device = VK_NULL_HANDLE;
    uint32_t compute_queue_family = 0;
    VkQueue compute_queue = VK_NULL_HANDLE;
    std::string device_name;
    DeviceInfo device_info;
};

struct VulkanComputePipeline {
    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkShaderModule shader_module = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
};

struct VulkanCommandPool {
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
};

struct VulkanQueryPool {
    VkQueryPool query_pool = VK_NULL_HANDLE;
    bool timing_supported = false;
};

// Helper function to find memory type for buffer allocation
static uint32_t FindMemoryType(VkPhysicalDevice physical_device, uint32_t type_filter, VkMemoryPropertyFlags properties) {
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
    
    // Map the entire buffer memory
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
    
    // Unmap the memory
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
    
    // Create buffer
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
    
    // Get memory requirements
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device_->logical_device, vk_buffer, &mem_requirements);
    
    // Determine memory properties - prefer host visible for CPU access
    VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    
    // Find suitable memory type
    uint32_t memory_type_index = FindMemoryType(device_->physical_device, 
                                               mem_requirements.memoryTypeBits, 
                                               memory_properties);
    
    // Allocate memory
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
    
    // Bind buffer to memory
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
    if (!device_ || !device_->logical_device) {
        return;
    }
    
    // Unmap if currently mapped
    if (is_mapped_) {
        Unmap();
    }
    
    // Clean up Vulkan resources
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

// VulkanKernelRunner implementation
VulkanKernelRunner::VulkanKernelRunner(const DeviceInfo& device_info) {
    InitializeVulkan(device_info);
}

VulkanKernelRunner::~VulkanKernelRunner() {
    ShutdownVulkan();
}

std::string VulkanKernelRunner::GetDeviceName() const {
    if (device_) {
        return device_->device_name;
    }
    return "Unknown Vulkan Device";
}

DeviceInfo VulkanKernelRunner::GetDeviceInfo() const {
    if (device_) {
        return device_->device_info;
    }
    
    DeviceInfo info;
    info.name = "Unknown Vulkan Device";
    info.backend_type = Backend::VULKAN;
    return info;
}

Result<void> VulkanKernelRunner::LoadKernel(const std::vector<uint8_t>& bytecode, const std::string& entry_point) {
    if (!device_ || !device_->logical_device) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Vulkan device not initialized");
    }
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Loading Vulkan kernel: " + entry_point + 
                      " (SPIR-V size: " + std::to_string(bytecode.size()) + " bytes)");
    
    if (bytecode.empty()) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::VALIDATION, ErrorCode::INVALID_ARGUMENT,
                                     "Empty SPIR-V bytecode");
    }
    
    // Validate SPIR-V bytecode size (must be multiple of 4 bytes)
    if (bytecode.size() % 4 != 0) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::VALIDATION, ErrorCode::INVALID_ARGUMENT,
                                     "Invalid SPIR-V bytecode: size must be multiple of 4 bytes");
    }
    
    // Create pipeline if it doesn't exist
    if (!pipeline_) {
        pipeline_ = std::make_unique<VulkanComputePipeline>();
    }
    
    // Clean up existing shader module if any
    if (pipeline_->shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_->logical_device, pipeline_->shader_module, nullptr);
        pipeline_->shader_module = VK_NULL_HANDLE;
    }
    
    // Create shader module from SPIR-V bytecode
    VkShaderModuleCreateInfo shader_info = {};
    shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_info.codeSize = bytecode.size();
    shader_info.pCode = reinterpret_cast<const uint32_t*>(bytecode.data());
    
    VkResult result = vkCreateShaderModule(device_->logical_device, &shader_info, nullptr, &pipeline_->shader_module);
    if (result != VK_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to create shader module: " + VulkanResultString(result));
    }
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, 
        "Vulkan shader module created successfully: " + entry_point + 
        " (" + std::to_string(bytecode.size()) + " bytes SPIR-V)");
    
    // Store entry point for pipeline creation
    entry_point_ = entry_point;
    
    // Create the compute pipeline now that we have the shader module
    auto pipeline_result = CreateComputePipeline();
    if (!pipeline_result) {
        return pipeline_result;
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> VulkanKernelRunner::CreateComputePipeline() {
    if (!device_ || !device_->logical_device || !pipeline_ || pipeline_->shader_module == VK_NULL_HANDLE) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Invalid device or shader module for pipeline creation");
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Creating Vulkan compute pipeline");
    
    // Clean up existing pipeline resources
    if (pipeline_->pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_->logical_device, pipeline_->pipeline, nullptr);
        pipeline_->pipeline = VK_NULL_HANDLE;
    }
    if (pipeline_->pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_->logical_device, pipeline_->pipeline_layout, nullptr);
        pipeline_->pipeline_layout = VK_NULL_HANDLE;
    }
    if (pipeline_->descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_->logical_device, pipeline_->descriptor_set_layout, nullptr);
        pipeline_->descriptor_set_layout = VK_NULL_HANDLE;
    }
    
    // Create descriptor set layout for 3 buffer bindings (typical for conv2d: input, output, constants)
    VkDescriptorSetLayoutBinding bindings[3] = {};
    
    // Binding 0: Input buffer (storage buffer)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].pImmutableSamplers = nullptr;
    
    // Binding 1: Output buffer (storage buffer)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].pImmutableSamplers = nullptr;
    
    // Binding 2: Constants buffer (uniform buffer)
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[2].pImmutableSamplers = nullptr;
    
    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 3;
    layout_info.pBindings = bindings;
    
    VkResult result = vkCreateDescriptorSetLayout(device_->logical_device, &layout_info, nullptr, &pipeline_->descriptor_set_layout);
    if (result != VK_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to create descriptor set layout: " + VulkanResultString(result));
    }
    
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &pipeline_->descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = nullptr;
    
    result = vkCreatePipelineLayout(device_->logical_device, &pipeline_layout_info, nullptr, &pipeline_->pipeline_layout);
    if (result != VK_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to create pipeline layout: " + VulkanResultString(result));
    }
    
    // Create compute pipeline
    VkPipelineShaderStageCreateInfo shader_stage = {};
    shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shader_stage.module = pipeline_->shader_module;
    shader_stage.pName = entry_point_.c_str();
    shader_stage.pSpecializationInfo = nullptr;
    
    VkComputePipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage = shader_stage;
    pipeline_info.layout = pipeline_->pipeline_layout;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;
    
    result = vkCreateComputePipelines(device_->logical_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline_->pipeline);
    if (result != VK_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to create compute pipeline: " + VulkanResultString(result));
    }
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Vulkan compute pipeline created successfully with entry point: " + entry_point_);
    
    // Create descriptor sets now that we have the pipeline and descriptor set layout
    auto descriptor_result = CreateDescriptorSets();
    if (!descriptor_result) {
        return descriptor_result;
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> VulkanKernelRunner::SetParameters(const void* params, size_t size) {
    if (!params || size == 0) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::VALIDATION, ErrorCode::INVALID_ARGUMENT,
                                     "Invalid parameters");
    }
    
    parameter_data_.resize(size);
    std::memcpy(parameter_data_.data(), params, size);
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Vulkan parameters set: " + std::to_string(size) + " bytes");
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> VulkanKernelRunner::SetBuffer(int binding, std::shared_ptr<IBuffer> buffer) {
    if (!buffer) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::VALIDATION, ErrorCode::INVALID_ARGUMENT,
                                     "Null buffer");
    }
    
    // VULKAN DEFERRED BINDING PATTERN:
    // Store buffer for later binding at dispatch time. This differs from CUDA's immediate binding.
    // Trade-off: Deferred error detection vs better performance (batch descriptor updates)
    // Vulkan best practice: Update all descriptor sets atomically before dispatch
    bound_buffers_[binding] = buffer;
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Vulkan buffer stored for deferred binding at dispatch: " + std::to_string(binding));
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> VulkanKernelRunner::SetTexture(int binding, std::shared_ptr<ITexture> texture) {
    if (!texture) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::VALIDATION, ErrorCode::INVALID_ARGUMENT,
                                     "Null texture");
    }
    
    bound_textures_[binding] = texture;
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Vulkan texture bound to binding " + std::to_string(binding));
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> VulkanKernelRunner::EnsureCommandBuffer() {
    if (!device_ || !device_->logical_device) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Vulkan device not initialized");
    }
    
    if (!command_pool_) {
        command_pool_ = std::make_unique<VulkanCommandPool>();
    }
    
    // Create command pool if it doesn't exist
    if (command_pool_->command_pool == VK_NULL_HANDLE) {
        VkCommandPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Allow command buffer reset
        pool_info.queueFamilyIndex = device_->compute_queue_family;
        
        VkResult result = vkCreateCommandPool(device_->logical_device, &pool_info, nullptr, &command_pool_->command_pool);
        if (result != VK_SUCCESS) {
            return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                         "Failed to create command pool: " + VulkanResultString(result));
        }
    }
    
    // Allocate command buffer if it doesn't exist
    if (command_pool_->command_buffer == VK_NULL_HANDLE) {
        VkCommandBufferAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = command_pool_->command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        
        VkResult result = vkAllocateCommandBuffers(device_->logical_device, &alloc_info, &command_pool_->command_buffer);
        if (result != VK_SUCCESS) {
            return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                         "Failed to allocate command buffer: " + VulkanResultString(result));
        }
        
        KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Vulkan command pool and buffer created");
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> VulkanKernelRunner::Dispatch(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) {
    if (!device_ || !device_->logical_device || !device_->compute_queue || !pipeline_ || 
        pipeline_->pipeline == VK_NULL_HANDLE || pipeline_->descriptor_set == VK_NULL_HANDLE) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Vulkan device, queue, pipeline, or descriptor set not initialized");
    }
    
    // Ensure command buffer is created
    auto cmd_result = EnsureCommandBuffer();
    if (!cmd_result) {
        return cmd_result;
    }
    
    // Update descriptor sets with bound buffers
    auto binding_result = UpdateDescriptorSets();
    if (!binding_result) {
        return binding_result;
    }
    
    dispatch_start_ = std::chrono::high_resolution_clock::now();
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Vulkan dispatch: " + 
                      std::to_string(groups_x) + "x" + std::to_string(groups_y) + "x" + std::to_string(groups_z));
    
    VkCommandBuffer cmd_buffer = command_pool_->command_buffer;
    
    // Begin command buffer recording
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    VkResult result = vkBeginCommandBuffer(cmd_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to begin command buffer: " + VulkanResultString(result));
    }
    
    // Bind compute pipeline
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_->pipeline);
    
    // Bind descriptor sets
    vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_->pipeline_layout,
                           0, 1, &pipeline_->descriptor_set, 0, nullptr);
    
    // Record dispatch command
    vkCmdDispatch(cmd_buffer, groups_x, groups_y, groups_z);
    
    // End command buffer recording
    result = vkEndCommandBuffer(cmd_buffer);
    if (result != VK_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to end command buffer: " + VulkanResultString(result));
    }
    
    // Create fence for synchronization
    VkFence fence;
    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    
    result = vkCreateFence(device_->logical_device, &fence_info, nullptr, &fence);
    if (result != VK_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to create fence: " + VulkanResultString(result));
    }
    
    // Submit command buffer
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buffer;
    
    result = vkQueueSubmit(device_->compute_queue, 1, &submit_info, fence);
    if (result != VK_SUCCESS) {
        vkDestroyFence(device_->logical_device, fence, nullptr);
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to submit command buffer: " + VulkanResultString(result));
    }
    
    // Wait for execution to complete
    result = vkWaitForFences(device_->logical_device, 1, &fence, VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS) {
        vkDestroyFence(device_->logical_device, fence, nullptr);
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to wait for fence: " + VulkanResultString(result));
    }
    
    // Clean up fence
    vkDestroyFence(device_->logical_device, fence, nullptr);
    
    dispatch_end_ = std::chrono::high_resolution_clock::now();
    
    // Update timing results
    auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(dispatch_end_ - dispatch_start_);
    last_timing_.compute_time_ms = total_duration.count() / 1000.0f;
    last_timing_.total_time_ms = last_timing_.compute_time_ms;
    last_timing_.memory_setup_time_ms = 0.1f; // Command buffer overhead
    last_timing_.memory_teardown_time_ms = 0.1f;
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Vulkan dispatch completed in " + 
                      std::to_string(last_timing_.compute_time_ms) + "ms (real GPU execution)");
    
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> VulkanKernelRunner::WaitForCompletion() {
    // In simplified implementation, dispatch is synchronous
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Vulkan wait for completion (synchronous)");
    return KERNTOPIA_VOID_SUCCESS();
}

TimingResults VulkanKernelRunner::GetLastExecutionTime() {
    return last_timing_;
}

Result<std::shared_ptr<IBuffer>> VulkanKernelRunner::CreateBuffer(size_t size, IBuffer::Type type, IBuffer::Usage usage) {
    if (!device_) {
        return KERNTOPIA_RESULT_ERROR(std::shared_ptr<IBuffer>, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Vulkan device not initialized");
    }
    
    if (size == 0) {
        return KERNTOPIA_RESULT_ERROR(std::shared_ptr<IBuffer>, ErrorCategory::VALIDATION, ErrorCode::INVALID_ARGUMENT,
                                     "Buffer size cannot be zero");
    }
    
    auto buffer = std::make_shared<VulkanBuffer>(device_.get(), size, type, usage);
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Created Vulkan buffer: " + std::to_string(size) + " bytes");
    return Result<std::shared_ptr<IBuffer>>::Success(buffer);
}

Result<std::shared_ptr<ITexture>> VulkanKernelRunner::CreateTexture(const TextureDesc& desc) {
    if (!device_) {
        return KERNTOPIA_RESULT_ERROR(std::shared_ptr<ITexture>, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Vulkan device not initialized");
    }
    
    if (desc.width == 0 || desc.height == 0) {
        return KERNTOPIA_RESULT_ERROR(std::shared_ptr<ITexture>, ErrorCategory::VALIDATION, ErrorCode::INVALID_ARGUMENT,
                                     "Texture dimensions cannot be zero");
    }
    
    auto texture = std::make_shared<VulkanTexture>(device_.get(), desc);
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Created Vulkan texture: " + 
                       std::to_string(desc.width) + "x" + std::to_string(desc.height));
    return Result<std::shared_ptr<ITexture>>::Success(texture);
}

void VulkanKernelRunner::CalculateDispatchSize(uint32_t width, uint32_t height, uint32_t depth,
                                              uint32_t& groups_x, uint32_t& groups_y, uint32_t& groups_z) {
    groups_x = (width + 15) / 16;
    groups_y = (height + 15) / 16;
    groups_z = depth;
}

std::string VulkanKernelRunner::GetDebugInfo() const {
    return "Vulkan backend (placeholder implementation)";
}

bool VulkanKernelRunner::SupportsFeature(const std::string& feature) const {
    return false; // Placeholder
}

Result<void> VulkanKernelRunner::SetSlangGlobalParameters(const void* params, size_t size) {
    // NOTE: Vulkan uses descriptor set binding via SetBuffer() instead of parameter binding
    // For Vulkan backends, constants should be bound using:
    //   SetBuffer(binding_index, constants_buffer)
    // This differs from CUDA which uses pointer-based parameter binding via SetSlangGlobalParameters
    //
    // Future backend implementers:
    // - CPU backends: May need direct memory copy approach
    // - Metal backends: Use buffer binding similar to Vulkan
    // - DirectX backends: Use constant buffer binding similar to Vulkan
    // - Custom backends: Choose pattern appropriate for your graphics API
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, 
        "Vulkan: SetSlangGlobalParameters is no-op (" + std::to_string(size) + 
        " bytes ignored, using SetBuffer() for descriptor binding instead)");
    
    return KERNTOPIA_VOID_SUCCESS();  // No-op for Vulkan - use SetBuffer() instead
}

// VulkanKernelRunnerFactory implementation
bool VulkanKernelRunnerFactory::IsAvailable() const {
    auto load_result = LoadVulkanLoader();
    if (!load_result) {
        KERNTOPIA_LOG_WARNING(LogComponent::BACKEND, "Vulkan loader not available: " + load_result.GetError().message);
        return false;
    }
    
    // For now, if we can load the Vulkan library successfully, assume it's available
    // This avoids potential ABI issues with structure definitions during detection phase
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Vulkan loader successfully loaded - marking as available");
    return true;
}

std::vector<DeviceInfo> VulkanKernelRunnerFactory::EnumerateDevices() const {
    auto load_result = LoadVulkanLoader();
    if (!load_result) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "Cannot enumerate Vulkan devices: " + load_result.GetError().message);
        return {};
    }
    
    // Use SystemInterrogator to get the actual detected Vulkan devices
    // This avoids duplicating device detection logic
    auto system_info_result = SystemInterrogator::GetSystemInfo();
    if (!system_info_result.HasValue()) {
        KERNTOPIA_LOG_WARNING(LogComponent::BACKEND, "SystemInterrogator failed to get system info: " + 
                             system_info_result.GetError().message);
        return {};
    }
    
    auto system_info = system_info_result.GetValue();
    
    // Extract Vulkan devices from system info
    std::vector<DeviceInfo> devices = system_info.vulkan_runtime.devices;
    
    if (devices.empty()) {
        KERNTOPIA_LOG_WARNING(LogComponent::BACKEND, "No Vulkan devices found in system interrogation");
        return {};
    }
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Found " + std::to_string(devices.size()) + " Vulkan devices via SystemInterrogator");
    
    for (size_t i = 0; i < devices.size(); ++i) {
        KERNTOPIA_LOG_INFO(LogComponent::BACKEND, 
            "Vulkan Device " + std::to_string(i) + ": " + devices[i].name + 
            " (" + devices[i].api_version + ", " + 
            std::to_string(devices[i].total_memory_bytes / (1024*1024)) + " MB)");
    }
    
    return devices;
}

Result<std::unique_ptr<IKernelRunner>> VulkanKernelRunnerFactory::CreateRunner(int device_id) const {
    auto load_result = LoadVulkanLoader();
    if (!load_result) {
        return KERNTOPIA_RESULT_ERROR(std::unique_ptr<IKernelRunner>, ErrorCategory::BACKEND,
                                     ErrorCode::BACKEND_NOT_AVAILABLE, "Vulkan loader not available");
    }
    
    // Validate device ID by enumerating devices
    auto devices = EnumerateDevices();
    if (device_id < 0 || device_id >= static_cast<int>(devices.size())) {
        return KERNTOPIA_RESULT_ERROR(std::unique_ptr<IKernelRunner>, ErrorCategory::VALIDATION,
                                     ErrorCode::INVALID_ARGUMENT, 
                                     "Invalid Vulkan device ID: " + std::to_string(device_id));
    }
    
    // Get the actual device info detected by SystemInterrogator
    const DeviceInfo& selected_device = devices[device_id];
    
    // Create and initialize runner with the selected device info
    std::unique_ptr<IKernelRunner> runner = std::make_unique<VulkanKernelRunner>(selected_device);
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Created Vulkan kernel runner for device " + std::to_string(device_id) + 
                      ": " + selected_device.name);
    return Result<std::unique_ptr<IKernelRunner>>::Success(std::move(runner));
}

std::string VulkanKernelRunnerFactory::GetVersion() const {
    auto load_result = LoadVulkanLoader();
    if (!load_result) {
        return "Not Available";
    }
    
    // TODO: Query actual Vulkan instance version
    return "Vulkan Loader (Dynamic)";
}

// VulkanKernelRunner implementation methods
bool VulkanKernelRunner::InitializeVulkan(const DeviceInfo& device_info) {
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Initializing Vulkan backend for device: " + device_info.name);
    
    // Ensure Vulkan loader is loaded
    auto load_result = LoadVulkanLoader();
    if (!load_result) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "Failed to load Vulkan loader: " + load_result.GetError().message);
        return false;
    }
    
    // Initialize context
    context_ = std::make_unique<VulkanContext>();
    
    // Verify that Vulkan functions are loaded
    if (!vkCreateInstance) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "vkCreateInstance function pointer is null - Vulkan loader failed");
        return false;
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Creating Vulkan instance...");
    
    // Create minimal Vulkan instance for compute (no validation layers, no extensions)
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Kerntopia";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "Kerntopia";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;
    
    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Calling vkCreateInstance...");
    VkResult result = vkCreateInstance(&instance_info, nullptr, &context_->instance);
    if (result != VK_SUCCESS) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "Failed to create Vulkan instance: " + VulkanResultString(result));
        return false;
    }
    
    // Enumerate physical devices and select by device_id (following CUDA pattern)
    uint32_t device_count = 0;
    result = vkEnumeratePhysicalDevices(context_->instance, &device_count, nullptr);
    if (result != VK_SUCCESS || device_count == 0) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "No Vulkan devices found: " + VulkanResultString(result));
        return false;
    }
    
    std::vector<VkPhysicalDevice> physical_devices(device_count);
    result = vkEnumeratePhysicalDevices(context_->instance, &device_count, physical_devices.data());
    if (result != VK_SUCCESS) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "Failed to enumerate physical devices: " + VulkanResultString(result));
        return false;
    }
    
    // Use device_id to select physical device (like CUDA does)
    int device_id = device_info.device_id >= 0 ? device_info.device_id : 0;
    if (device_id >= static_cast<int>(device_count)) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "Invalid device ID " + std::to_string(device_id) + 
                           " (only " + std::to_string(device_count) + " devices available)");
        return false;
    }
    
    // Initialize device
    device_ = std::make_unique<VulkanDevice>();
    device_->physical_device = physical_devices[device_id];
    device_->device_name = device_info.name;
    device_->device_info = device_info;
    
    // Find compute queue family
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device_->physical_device, &queue_family_count, nullptr);
    
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device_->physical_device, &queue_family_count, queue_families.data());
    
    device_->compute_queue_family = UINT32_MAX;
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            device_->compute_queue_family = i;
            break;
        }
    }
    
    if (device_->compute_queue_family == UINT32_MAX) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "No compute queue family found on device");
        return false;
    }
    
    // Create logical device with compute queue
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = device_->compute_queue_family;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;
    
    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    
    result = vkCreateDevice(device_->physical_device, &device_create_info, nullptr, &device_->logical_device);
    if (result != VK_SUCCESS) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "Failed to create logical device: " + VulkanResultString(result));
        return false;
    }
    
    // Get compute queue handle
    vkGetDeviceQueue(device_->logical_device, device_->compute_queue_family, 0, &device_->compute_queue);
    
    // Initialize other components
    command_pool_ = std::make_unique<VulkanCommandPool>();
    query_pool_ = std::make_unique<VulkanQueryPool>();
    query_pool_->timing_supported = false;
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Vulkan backend initialized successfully: " + device_info.name + 
                      " (device " + std::to_string(device_id) + ", queue family " + std::to_string(device_->compute_queue_family) + ")");
    return true;
}

void VulkanKernelRunner::ShutdownVulkan() {
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Shutting down Vulkan backend");
    
    // Clear bindings
    bound_buffers_.clear();
    bound_textures_.clear();
    parameter_data_.clear();
    
    // Destroy Vulkan resources in reverse creation order
    if (device_ && device_->logical_device) {
        // Destroy pipeline resources
        if (pipeline_) {
            if (pipeline_->descriptor_pool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device_->logical_device, pipeline_->descriptor_pool, nullptr);
            }
            if (pipeline_->pipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(device_->logical_device, pipeline_->pipeline, nullptr);
            }
            if (pipeline_->pipeline_layout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(device_->logical_device, pipeline_->pipeline_layout, nullptr);
            }
            if (pipeline_->descriptor_set_layout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(device_->logical_device, pipeline_->descriptor_set_layout, nullptr);
            }
            if (pipeline_->shader_module != VK_NULL_HANDLE) {
                vkDestroyShaderModule(device_->logical_device, pipeline_->shader_module, nullptr);
            }
        }
        
        // Destroy command pool
        if (command_pool_ && command_pool_->command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_->logical_device, command_pool_->command_pool, nullptr);
        }
        
        // Destroy logical device
        vkDestroyDevice(device_->logical_device, nullptr);
    }
    
    // Destroy instance
    if (context_ && context_->instance != VK_NULL_HANDLE) {
        vkDestroyInstance(context_->instance, nullptr);
    }
    
    // Release resources
    query_pool_.reset();
    command_pool_.reset();
    pipeline_.reset();
    device_.reset();
    context_.reset();
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Vulkan backend shutdown complete");
}

Result<void> VulkanKernelRunner::CreateDescriptorSets() {
    if (!device_ || !device_->logical_device || !pipeline_ || pipeline_->descriptor_set_layout == VK_NULL_HANDLE) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Invalid device or descriptor set layout for descriptor set creation");
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Creating Vulkan descriptor sets with pool");
    
    // Clean up existing descriptor pool and sets
    if (pipeline_->descriptor_set != VK_NULL_HANDLE) {
        // Note: descriptor sets are automatically freed when pool is destroyed
        pipeline_->descriptor_set = VK_NULL_HANDLE;
    }
    if (pipeline_->descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_->logical_device, pipeline_->descriptor_pool, nullptr);
        pipeline_->descriptor_pool = VK_NULL_HANDLE;
    }
    
    // Create descriptor pool with sizes for our 3 bindings (2 storage buffers + 1 uniform buffer)
    VkDescriptorPoolSize pool_sizes[2] = {};
    
    // Storage buffers (bindings 0 and 1: input and output)
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_sizes[0].descriptorCount = 2; // input and output buffers
    
    // Uniform buffer (binding 2: constants)
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[1].descriptorCount = 1; // constants buffer
    
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; // Allow individual set freeing
    pool_info.maxSets = 1; // We only need 1 descriptor set
    pool_info.poolSizeCount = 2;
    pool_info.pPoolSizes = pool_sizes;
    
    VkResult result = vkCreateDescriptorPool(device_->logical_device, &pool_info, nullptr, &pipeline_->descriptor_pool);
    if (result != VK_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to create descriptor pool: " + VulkanResultString(result));
    }
    
    // Allocate descriptor set from the pool
    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = pipeline_->descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &pipeline_->descriptor_set_layout;
    
    result = vkAllocateDescriptorSets(device_->logical_device, &alloc_info, &pipeline_->descriptor_set);
    if (result != VK_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to allocate descriptor sets: " + VulkanResultString(result));
    }
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Vulkan descriptor pool and sets created successfully");
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> VulkanKernelRunner::UpdateDescriptorSets() {
    if (!device_ || !device_->logical_device || !pipeline_ || pipeline_->descriptor_set == VK_NULL_HANDLE) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Invalid device or descriptor set for buffer binding");
    }
    
    if (bound_buffers_.empty()) {
        KERNTOPIA_LOG_WARNING(LogComponent::BACKEND, "No buffers bound - skipping descriptor set update");
        return KERNTOPIA_VOID_SUCCESS();
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Updating Vulkan descriptor sets with " + std::to_string(bound_buffers_.size()) + " buffers");
    
    // Create descriptor writes for each bound buffer
    std::vector<VkWriteDescriptorSet> descriptor_writes;
    std::vector<VkDescriptorBufferInfo> buffer_infos;
    
    // Reserve space to avoid reallocations
    descriptor_writes.reserve(bound_buffers_.size());
    buffer_infos.reserve(bound_buffers_.size());
    
    for (const auto& [binding_index, buffer] : bound_buffers_) {
        // Cast to VulkanBuffer to get VkBuffer handle
        auto vulkan_buffer = std::dynamic_pointer_cast<VulkanBuffer>(buffer);
        if (!vulkan_buffer) {
            return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::VALIDATION, ErrorCode::INVALID_ARGUMENT,
                                         "Buffer at binding " + std::to_string(binding_index) + " is not a VulkanBuffer");
        }
        
        // Get VkBuffer handle (stored as void* in GetBuffer())
        VkBuffer vk_buffer = static_cast<VkBuffer>(vulkan_buffer->GetBuffer());
        if (vk_buffer == nullptr) {
            return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                         "Invalid VkBuffer at binding " + std::to_string(binding_index));
        }
        
        // Create buffer info
        VkDescriptorBufferInfo& buffer_info = buffer_infos.emplace_back();
        buffer_info.buffer = vk_buffer;
        buffer_info.offset = 0;
        buffer_info.range = vulkan_buffer->GetSize();
        
        // Create write descriptor set
        VkWriteDescriptorSet& write = descriptor_writes.emplace_back();
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = nullptr;
        write.dstSet = pipeline_->descriptor_set;
        write.dstBinding = binding_index;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        
        // Determine descriptor type based on binding index
        if (binding_index == 2) {
            // Binding 2 is constants buffer (uniform buffer)
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        } else {
            // Bindings 0 and 1 are storage buffers (input/output)
            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
        
        write.pBufferInfo = &buffer_info;
        write.pImageInfo = nullptr;
        write.pTexelBufferView = nullptr;
        
        KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, 
            "Prepared descriptor write for binding " + std::to_string(binding_index) + 
            " (buffer size: " + std::to_string(vulkan_buffer->GetSize()) + " bytes)");
    }
    
    // Update all descriptor sets at once
    vkUpdateDescriptorSets(device_->logical_device, 
                          static_cast<uint32_t>(descriptor_writes.size()), 
                          descriptor_writes.data(), 
                          0, nullptr);
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, 
        "Successfully updated Vulkan descriptor sets with " + std::to_string(descriptor_writes.size()) + " buffer bindings");
    
    return KERNTOPIA_VOID_SUCCESS();
}

} // namespace kerntopia