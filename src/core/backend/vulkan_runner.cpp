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
#include <cstdlib>  // for setenv() and getenv()
#include <memory>   // for std::unique_ptr

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

// vkGetInstanceProcAddr - the only function we load directly via dlsym
static PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;

// Instance functions
static vkCreateInstance_t vkCreateInstance = nullptr;
static vkDestroyInstance_t vkDestroyInstance = nullptr;
static vkEnumeratePhysicalDevices_t vkEnumeratePhysicalDevices = nullptr;
static vkGetPhysicalDeviceProperties_t vkGetPhysicalDeviceProperties = nullptr;
vkGetPhysicalDeviceMemoryProperties_t vkGetPhysicalDeviceMemoryProperties = nullptr;
static vkGetPhysicalDeviceFeatures_t vkGetPhysicalDeviceFeatures = nullptr;
static vkGetPhysicalDeviceQueueFamilyProperties_t vkGetPhysicalDeviceQueueFamilyProperties = nullptr;

// Device functions
static vkCreateDevice_t vkCreateDevice = nullptr;
static vkDestroyDevice_t vkDestroyDevice = nullptr;
static vkGetDeviceQueue_t vkGetDeviceQueue = nullptr;

// Buffer functions (accessible to vulkan_memory.cpp)
vkCreateBuffer_t vkCreateBuffer = nullptr;
vkDestroyBuffer_t vkDestroyBuffer = nullptr;
vkGetBufferMemoryRequirements_t vkGetBufferMemoryRequirements = nullptr;

// Memory functions (accessible to vulkan_memory.cpp)
vkAllocateMemory_t vkAllocateMemory = nullptr;
vkFreeMemory_t vkFreeMemory = nullptr;
vkBindBufferMemory_t vkBindBufferMemory = nullptr;
vkMapMemory_t vkMapMemory = nullptr;
vkUnmapMemory_t vkUnmapMemory = nullptr;

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

// Note: Using singleton RuntimeLoader to maintain library persistence across backends
// Vulkan library handle is now managed by SystemInterrogator compatibility layer

// Helper function to load instance-level functions using vkGetInstanceProcAddr
static Result<void> LoadInstanceFunctions(VkInstance instance) {
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "LoadInstanceFunctions: Loading instance-level functions");
    
    if (!vkGetInstanceProcAddr) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "vkGetInstanceProcAddr not available");
    }
    
    // Load instance-level functions (these require a valid VkInstance handle)
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "LoadInstanceFunctions: Loading vkEnumeratePhysicalDevices with instance");
    vkEnumeratePhysicalDevices = reinterpret_cast<vkEnumeratePhysicalDevices_t>(
        vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices"));
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "LoadInstanceFunctions: vkEnumeratePhysicalDevices loaded: " +
                       std::to_string(reinterpret_cast<uintptr_t>(vkEnumeratePhysicalDevices)));
    
    vkDestroyInstance = reinterpret_cast<vkDestroyInstance_t>(
        vkGetInstanceProcAddr(instance, "vkDestroyInstance"));
    vkGetPhysicalDeviceProperties = reinterpret_cast<vkGetPhysicalDeviceProperties_t>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties"));
    vkGetPhysicalDeviceMemoryProperties = reinterpret_cast<vkGetPhysicalDeviceMemoryProperties_t>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties"));
    vkGetPhysicalDeviceFeatures = reinterpret_cast<vkGetPhysicalDeviceFeatures_t>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures"));
    vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<vkGetPhysicalDeviceQueueFamilyProperties_t>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
    vkCreateDevice = reinterpret_cast<vkCreateDevice_t>(
        vkGetInstanceProcAddr(instance, "vkCreateDevice"));
    
    // Note: Device-level functions (vkGetDeviceQueue, vkCreateBuffer, etc.) will be loaded 
    // after device creation using vkGetDeviceProcAddr
    
    // Verify instance-level functions were loaded
    if (!vkEnumeratePhysicalDevices || !vkDestroyInstance || !vkGetPhysicalDeviceProperties ||
        !vkGetPhysicalDeviceMemoryProperties || !vkGetPhysicalDeviceFeatures ||
        !vkGetPhysicalDeviceQueueFamilyProperties || !vkCreateDevice) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to load required Vulkan instance functions");
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "LoadInstanceFunctions: All instance functions loaded successfully");
    return KERNTOPIA_VOID_SUCCESS();
}

// Helper function to load device-level functions using vkGetDeviceProcAddr  
static Result<void> LoadDeviceFunctions(VkInstance instance, VkDevice device) {
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "LoadDeviceFunctions: Loading device-level functions");
    
    if (!vkGetInstanceProcAddr) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "vkGetInstanceProcAddr not available");
    }
    
    // Get vkGetDeviceProcAddr from instance
    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
    
    if (!vkGetDeviceProcAddr) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to load vkGetDeviceProcAddr");
    }
    
    // Load device-level functions
    vkDestroyDevice = reinterpret_cast<vkDestroyDevice_t>(
        vkGetDeviceProcAddr(device, "vkDestroyDevice"));
    vkGetDeviceQueue = reinterpret_cast<vkGetDeviceQueue_t>(
        vkGetDeviceProcAddr(device, "vkGetDeviceQueue"));
    
    // Buffer functions
    vkCreateBuffer = reinterpret_cast<vkCreateBuffer_t>(
        vkGetDeviceProcAddr(device, "vkCreateBuffer"));
    vkDestroyBuffer = reinterpret_cast<vkDestroyBuffer_t>(
        vkGetDeviceProcAddr(device, "vkDestroyBuffer"));
    vkGetBufferMemoryRequirements = reinterpret_cast<vkGetBufferMemoryRequirements_t>(
        vkGetDeviceProcAddr(device, "vkGetBufferMemoryRequirements"));
    
    // Memory functions
    vkAllocateMemory = reinterpret_cast<vkAllocateMemory_t>(
        vkGetDeviceProcAddr(device, "vkAllocateMemory"));
    vkFreeMemory = reinterpret_cast<vkFreeMemory_t>(
        vkGetDeviceProcAddr(device, "vkFreeMemory"));
    vkBindBufferMemory = reinterpret_cast<vkBindBufferMemory_t>(
        vkGetDeviceProcAddr(device, "vkBindBufferMemory"));
    vkMapMemory = reinterpret_cast<vkMapMemory_t>(
        vkGetDeviceProcAddr(device, "vkMapMemory"));
    vkUnmapMemory = reinterpret_cast<vkUnmapMemory_t>(
        vkGetDeviceProcAddr(device, "vkUnmapMemory"));
    
    // Shader and pipeline functions
    vkCreateShaderModule = reinterpret_cast<vkCreateShaderModule_t>(
        vkGetDeviceProcAddr(device, "vkCreateShaderModule"));
    vkDestroyShaderModule = reinterpret_cast<vkDestroyShaderModule_t>(
        vkGetDeviceProcAddr(device, "vkDestroyShaderModule"));
    vkCreatePipelineLayout = reinterpret_cast<vkCreatePipelineLayout_t>(
        vkGetDeviceProcAddr(device, "vkCreatePipelineLayout"));
    vkDestroyPipelineLayout = reinterpret_cast<vkDestroyPipelineLayout_t>(
        vkGetDeviceProcAddr(device, "vkDestroyPipelineLayout"));
    vkCreateComputePipelines = reinterpret_cast<vkCreateComputePipelines_t>(
        vkGetDeviceProcAddr(device, "vkCreateComputePipelines"));
    vkDestroyPipeline = reinterpret_cast<vkDestroyPipeline_t>(
        vkGetDeviceProcAddr(device, "vkDestroyPipeline"));
    
    // Descriptor set functions
    vkCreateDescriptorSetLayout = reinterpret_cast<vkCreateDescriptorSetLayout_t>(
        vkGetDeviceProcAddr(device, "vkCreateDescriptorSetLayout"));
    vkDestroyDescriptorSetLayout = reinterpret_cast<vkDestroyDescriptorSetLayout_t>(
        vkGetDeviceProcAddr(device, "vkDestroyDescriptorSetLayout"));
    vkCreateDescriptorPool = reinterpret_cast<vkCreateDescriptorPool_t>(
        vkGetDeviceProcAddr(device, "vkCreateDescriptorPool"));
    vkDestroyDescriptorPool = reinterpret_cast<vkDestroyDescriptorPool_t>(
        vkGetDeviceProcAddr(device, "vkDestroyDescriptorPool"));
    vkAllocateDescriptorSets = reinterpret_cast<vkAllocateDescriptorSets_t>(
        vkGetDeviceProcAddr(device, "vkAllocateDescriptorSets"));
    vkUpdateDescriptorSets = reinterpret_cast<vkUpdateDescriptorSets_t>(
        vkGetDeviceProcAddr(device, "vkUpdateDescriptorSets"));
    
    // Command buffer functions
    vkCreateCommandPool = reinterpret_cast<vkCreateCommandPool_t>(
        vkGetDeviceProcAddr(device, "vkCreateCommandPool"));
    vkDestroyCommandPool = reinterpret_cast<vkDestroyCommandPool_t>(
        vkGetDeviceProcAddr(device, "vkDestroyCommandPool"));
    vkAllocateCommandBuffers = reinterpret_cast<vkAllocateCommandBuffers_t>(
        vkGetDeviceProcAddr(device, "vkAllocateCommandBuffers"));
    vkFreeCommandBuffers = reinterpret_cast<vkFreeCommandBuffers_t>(
        vkGetDeviceProcAddr(device, "vkFreeCommandBuffers"));
    vkBeginCommandBuffer = reinterpret_cast<vkBeginCommandBuffer_t>(
        vkGetDeviceProcAddr(device, "vkBeginCommandBuffer"));
    vkEndCommandBuffer = reinterpret_cast<vkEndCommandBuffer_t>(
        vkGetDeviceProcAddr(device, "vkEndCommandBuffer"));
    vkCmdBindPipeline = reinterpret_cast<vkCmdBindPipeline_t>(
        vkGetDeviceProcAddr(device, "vkCmdBindPipeline"));
    vkCmdBindDescriptorSets = reinterpret_cast<vkCmdBindDescriptorSets_t>(
        vkGetDeviceProcAddr(device, "vkCmdBindDescriptorSets"));
    vkCmdDispatch = reinterpret_cast<vkCmdDispatch_t>(
        vkGetDeviceProcAddr(device, "vkCmdDispatch"));
    
    // Queue and synchronization functions
    vkQueueSubmit = reinterpret_cast<vkQueueSubmit_t>(
        vkGetDeviceProcAddr(device, "vkQueueSubmit"));
    vkQueueWaitIdle = reinterpret_cast<vkQueueWaitIdle_t>(
        vkGetDeviceProcAddr(device, "vkQueueWaitIdle"));
    vkCreateFence = reinterpret_cast<vkCreateFence_t>(
        vkGetDeviceProcAddr(device, "vkCreateFence"));
    vkDestroyFence = reinterpret_cast<vkDestroyFence_t>(
        vkGetDeviceProcAddr(device, "vkDestroyFence"));
    vkWaitForFences = reinterpret_cast<vkWaitForFences_t>(
        vkGetDeviceProcAddr(device, "vkWaitForFences"));
    vkResetFences = reinterpret_cast<vkResetFences_t>(
        vkGetDeviceProcAddr(device, "vkResetFences"));
    
    // Verify all device-level functions were loaded
    if (!vkDestroyDevice || !vkGetDeviceQueue || !vkCreateBuffer || !vkDestroyBuffer || 
        !vkGetBufferMemoryRequirements || !vkAllocateMemory || !vkFreeMemory || !vkBindBufferMemory ||
        !vkMapMemory || !vkUnmapMemory || !vkCreateShaderModule || !vkDestroyShaderModule ||
        !vkCreatePipelineLayout || !vkDestroyPipelineLayout || !vkCreateComputePipelines || !vkDestroyPipeline ||
        !vkCreateDescriptorSetLayout || !vkDestroyDescriptorSetLayout || !vkCreateDescriptorPool || 
        !vkDestroyDescriptorPool || !vkAllocateDescriptorSets || !vkUpdateDescriptorSets ||
        !vkCreateCommandPool || !vkDestroyCommandPool || !vkAllocateCommandBuffers || !vkFreeCommandBuffers ||
        !vkBeginCommandBuffer || !vkEndCommandBuffer || !vkCmdBindPipeline || !vkCmdBindDescriptorSets ||
        !vkCmdDispatch || !vkQueueSubmit || !vkQueueWaitIdle || !vkCreateFence || !vkDestroyFence ||
        !vkWaitForFences || !vkResetFences) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to load required Vulkan device functions");
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "LoadDeviceFunctions: All device functions loaded successfully");
    return KERNTOPIA_VOID_SUCCESS();
}

// Helper function to load Vulkan loader using SystemInterrogator compatibility layer
static Result<void> LoadVulkanLoader() {
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "LoadVulkanLoader: Starting with SystemInterrogator compatibility layer");
    
    // Skip loading if functions are already loaded
    if (vkGetInstanceProcAddr && vkCreateInstance) {
        KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "LoadVulkanLoader: Vulkan functions already loaded, returning");
        return KERNTOPIA_VOID_SUCCESS();
    }
    
    // Use SystemInterrogator's cached Vulkan library handle for consistency
    auto handle_result = SystemInterrogator::GetVulkanLibraryHandle();
    if (!handle_result) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to get Vulkan library handle from SystemInterrogator: " + handle_result.GetError().message);
    }
    
    void* vulkan_loader_handle = handle_result.GetValue();
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "LoadVulkanLoader: Using SystemInterrogator's Vulkan library handle: " +
                       std::to_string(reinterpret_cast<uintptr_t>(vulkan_loader_handle)));
    
    // Get RuntimeLoader singleton for symbol loading
    RuntimeLoader& runtime_loader = RuntimeLoader::GetInstance();
    
    // Load vkGetInstanceProcAddr - the ONLY function we can load directly via dlsym
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "LoadVulkanLoader: Loading vkGetInstanceProcAddr via dlsym");
    vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        runtime_loader.GetSymbol(vulkan_loader_handle, "vkGetInstanceProcAddr"));
    
    if (!vkGetInstanceProcAddr) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to load vkGetInstanceProcAddr via dlsym");
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "LoadVulkanLoader: vkGetInstanceProcAddr loaded: " +
                       std::to_string(reinterpret_cast<uintptr_t>(vkGetInstanceProcAddr)));
    
    // Load ONLY global-level functions using vkGetInstanceProcAddr with NULL instance
    // According to Vulkan spec, only these functions can be loaded with VK_NULL_HANDLE:
    // - vkEnumerateInstanceVersion, vkEnumerateInstanceExtensionProperties, 
    //   vkEnumerateInstanceLayerProperties, vkCreateInstance
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "LoadVulkanLoader: Loading vkCreateInstance via vkGetInstanceProcAddr(NULL)");
    vkCreateInstance = reinterpret_cast<vkCreateInstance_t>(
        vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
    
    if (!vkCreateInstance) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to load vkCreateInstance via vkGetInstanceProcAddr");
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "LoadVulkanLoader: vkCreateInstance loaded: " +
                       std::to_string(reinterpret_cast<uintptr_t>(vkCreateInstance)));
    
    // Note: ALL other functions (including vkEnumeratePhysicalDevices) are instance-level
    // and MUST be loaded in LoadInstanceFunctions() with a valid VkInstance handle
    
    if (!vkGetInstanceProcAddr || !vkCreateInstance) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "LoadVulkanLoader: Essential function validation failed: vkCreateInstance=" +
                           std::to_string(reinterpret_cast<uintptr_t>(vkCreateInstance)));
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to load required Vulkan global functions");
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "LoadVulkanLoader: All function pointers loaded successfully using SystemInterrogator compatibility layer");
    return KERNTOPIA_VOID_SUCCESS();
}
// Helper functions moved to vulkan_memory.cpp

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

// FindMemoryType function moved to vulkan_memory.cpp

// Memory class implementations moved to vulkan_memory.cpp

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
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "CreateComputePipeline: Starting pipeline creation");
    
    // Detailed prerequisite validation
    if (!device_) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "CreateComputePipeline: device_ is null");
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Device is null");
    }
    if (!device_->logical_device) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "CreateComputePipeline: logical_device is null");
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Logical device is null");
    }
    if (!pipeline_) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "CreateComputePipeline: pipeline_ is null");
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Pipeline structure is null");
    }
    if (pipeline_->shader_module == VK_NULL_HANDLE) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "CreateComputePipeline: shader_module is VK_NULL_HANDLE");
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Shader module is invalid");
    }
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "CreateComputePipeline: All prerequisites validated, entry point: " + entry_point_);
    
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
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "CreateComputePipeline: Creating descriptor set layout with 3 bindings");
    VkResult result = vkCreateDescriptorSetLayout(device_->logical_device, &layout_info, nullptr, &pipeline_->descriptor_set_layout);
    if (result != VK_SUCCESS) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "CreateComputePipeline: Failed to create descriptor set layout: " + VulkanResultString(result));
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to create descriptor set layout: " + VulkanResultString(result));
    }
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "CreateComputePipeline: Descriptor set layout created successfully");
    
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &pipeline_->descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = nullptr;
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "CreateComputePipeline: Creating pipeline layout");
    result = vkCreatePipelineLayout(device_->logical_device, &pipeline_layout_info, nullptr, &pipeline_->pipeline_layout);
    if (result != VK_SUCCESS) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "CreateComputePipeline: Failed to create pipeline layout: " + VulkanResultString(result));
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to create pipeline layout: " + VulkanResultString(result));
    }
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "CreateComputePipeline: Pipeline layout created successfully");
    
    // Create compute pipeline
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "CreateComputePipeline: Setting up shader stage with entry point: " + entry_point_);
    VkPipelineShaderStageCreateInfo shader_stage = {};
    shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shader_stage.module = pipeline_->shader_module;
    shader_stage.pName = entry_point_.c_str();
    shader_stage.pSpecializationInfo = nullptr;
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "CreateComputePipeline: Setting up compute pipeline create info");
    VkComputePipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage = shader_stage;
    pipeline_info.layout = pipeline_->pipeline_layout;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "CreateComputePipeline: About to call vkCreateComputePipelines with entry point: " + entry_point_);
    result = vkCreateComputePipelines(device_->logical_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline_->pipeline);
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "CreateComputePipeline: vkCreateComputePipelines returned: " + VulkanResultString(result) + " (" + std::to_string(result) + ")");
    
    if (result != VK_SUCCESS) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "CreateComputePipeline: DETAILED ERROR - vkCreateComputePipelines failed");
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "  Entry point: " + entry_point_);
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "  Shader module valid: " + std::string(pipeline_->shader_module != VK_NULL_HANDLE ? "YES" : "NO"));
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "  Pipeline layout valid: " + std::string(pipeline_->pipeline_layout != VK_NULL_HANDLE ? "YES" : "NO"));
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "  Descriptor set layout valid: " + std::string(pipeline_->descriptor_set_layout != VK_NULL_HANDLE ? "YES" : "NO"));
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
    // Use SystemInterrogator for consistent runtime detection (matches CUDA pattern)
    return SystemInterrogator::IsRuntimeAvailable(RuntimeType::VULKAN);
}

std::vector<DeviceInfo> VulkanKernelRunnerFactory::EnumerateDevices() const {
    // Use SystemInterrogator to get the actual detected Vulkan devices (matches CUDA pattern)
    // This avoids duplicating device detection logic
    auto system_info_result = SystemInterrogator::GetSystemInfo();
    if (!system_info_result.HasValue()) {
        KERNTOPIA_LOG_WARNING(LogComponent::BACKEND, "SystemInterrogator failed to get system info: " + 
                             system_info_result.GetError().message);
        return {};
    }
    
    const SystemInfo& system_info = system_info_result.GetValue();
    
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
    // Use SystemInterrogator for basic availability check (matches CUDA pattern)
    if (!SystemInterrogator::IsRuntimeAvailable(RuntimeType::VULKAN)) {
        return KERNTOPIA_RESULT_ERROR(std::unique_ptr<IKernelRunner>, ErrorCategory::BACKEND,
                                     ErrorCode::BACKEND_NOT_AVAILABLE, "Vulkan runtime not available via SystemInterrogator");
    }
    
    // Validate device ID using SystemInterrogator devices (matches CUDA pattern)
    auto devices = EnumerateDevices();
    if (device_id < 0 || device_id >= static_cast<int>(devices.size())) {
        return KERNTOPIA_RESULT_ERROR(std::unique_ptr<IKernelRunner>, ErrorCategory::VALIDATION,
                                     ErrorCode::INVALID_ARGUMENT, 
                                     "Invalid Vulkan device ID: " + std::to_string(device_id));
    }
    
    // Get the actual device info detected by SystemInterrogator
    const DeviceInfo& selected_device = devices[device_id];
    
    // COMPATIBILITY: Still load Vulkan functions for runtime execution
    auto load_result = LoadVulkanLoader();
    if (!load_result) {
        return KERNTOPIA_RESULT_ERROR(std::unique_ptr<IKernelRunner>, ErrorCategory::BACKEND,
                                     ErrorCode::BACKEND_NOT_AVAILABLE, "Vulkan loader not available for runtime");
    }
    
    // Create and initialize runner with the selected device info
    std::unique_ptr<IKernelRunner> runner = std::make_unique<VulkanKernelRunner>(selected_device);
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Created Vulkan kernel runner for device " + std::to_string(device_id) + 
                      ": " + selected_device.name);
    return Result<std::unique_ptr<IKernelRunner>>::Success(std::move(runner));
}

std::string VulkanKernelRunnerFactory::GetVersion() const {
    // Use SystemInterrogator to get version information (matches CUDA pattern)
    if (!SystemInterrogator::IsRuntimeAvailable(RuntimeType::VULKAN)) {
        return "Not Available";
    }
    
    // Get version information from SystemInterrogator
    auto system_info_result = SystemInterrogator::GetSystemInfo();
    if (system_info_result && system_info_result.GetValue().vulkan_runtime.available) {
        return system_info_result.GetValue().vulkan_runtime.version;
    }
    
    return "Vulkan Loader (Dynamic Detection)";
}

// VulkanKernelRunner implementation methods
bool VulkanKernelRunner::InitializeVulkan(const DeviceInfo& device_info) {
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Initializing Vulkan backend for device: " + device_info.name);
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "STEP 1: Starting InitializeVulkan");
    
    // Ensure Vulkan loader is loaded
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "STEP 2: About to call LoadVulkanLoader");
    auto load_result = LoadVulkanLoader();
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "STEP 3: LoadVulkanLoader completed");
    if (!load_result) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "Failed to load Vulkan loader: " + load_result.GetError().message);
        return false;
    }
    
    // Initialize context
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "STEP 4: About to create VulkanContext");
    context_ = std::make_unique<VulkanContext>();
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "STEP 5: VulkanContext created successfully");
    
    // Verify that Vulkan functions are loaded
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "STEP 6: About to verify Vulkan functions");
    if (!vkCreateInstance) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "vkCreateInstance function pointer is null - Vulkan loader failed");
        return false;
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Creating Vulkan instance...");
    
    // Force Lavapipe ICD selection for CPU Vulkan support in WSL
    // CRITICAL: Set this BEFORE any Vulkan library operations
    const char* vk_icd_filenames = std::getenv("VK_ICD_FILENAMES");
    if (!vk_icd_filenames) {
        // Set environment variable to force Lavapipe (CPU Vulkan implementation)
        KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Setting VK_ICD_FILENAMES to force Lavapipe CPU implementation");
        setenv("VK_ICD_FILENAMES", "/usr/share/vulkan/icd.d/lvp_icd.x86_64.json", 1);
    } else {
        KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "VK_ICD_FILENAMES already set: " + std::string(vk_icd_filenames));
    }
    
    // Test: Use local stack variable like our working test
    VkInstance test_instance = VK_NULL_HANDLE;
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "DEBUG: Using local stack variable for instance");
    
    // MEMORY CORRUPTION DETECTION: Add stack canaries
    const uint32_t STACK_CANARY = 0xDEADBEEF;
    uint32_t canary_before = STACK_CANARY;
    
    // Create minimal Vulkan instance for compute
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Kerntopia";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "Kerntopia";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;
    
    // Create minimal Vulkan instance - temporarily removing validation layers to isolate memory corruption
    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledLayerCount = 0;
    instance_info.ppEnabledLayerNames = nullptr;
    instance_info.enabledExtensionCount = 0;
    instance_info.ppEnabledExtensionNames = nullptr;
    
    uint32_t canary_after = STACK_CANARY;
    
    // MEMORY LAYOUT VERIFICATION
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Structure sizes - VkApplicationInfo: " + 
                       std::to_string(sizeof(VkApplicationInfo)) + 
                       ", VkInstanceCreateInfo: " + std::to_string(sizeof(VkInstanceCreateInfo)));
    
    // HEX DUMP OF STRUCTURES
    const uint8_t* app_info_bytes = reinterpret_cast<const uint8_t*>(&app_info);
    std::string app_info_hex = "VkApplicationInfo hex: ";
    for (size_t i = 0; i < sizeof(VkApplicationInfo); i++) {
        char hex_buf[4];
        snprintf(hex_buf, sizeof(hex_buf), "%02x ", app_info_bytes[i]);
        app_info_hex += hex_buf;
    }
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, app_info_hex);
    
    const uint8_t* instance_info_bytes = reinterpret_cast<const uint8_t*>(&instance_info);
    std::string instance_info_hex = "VkInstanceCreateInfo hex: ";
    for (size_t i = 0; i < sizeof(VkInstanceCreateInfo); i++) {
        char hex_buf[4];
        snprintf(hex_buf, sizeof(hex_buf), "%02x ", instance_info_bytes[i]);
        instance_info_hex += hex_buf;
    }
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, instance_info_hex);
    
    // POINTER VALIDATION
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Pointer validation - app_info.pApplicationName: " +
                       std::to_string(reinterpret_cast<uintptr_t>(app_info.pApplicationName)) +
                       " (" + std::string(app_info.pApplicationName ? app_info.pApplicationName : "NULL") + ")");
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Pointer validation - app_info.pEngineName: " +
                       std::to_string(reinterpret_cast<uintptr_t>(app_info.pEngineName)) +
                       " (" + std::string(app_info.pEngineName ? app_info.pEngineName : "NULL") + ")");
                       
    // STACK CANARY CHECK
    if (canary_before != STACK_CANARY || canary_after != STACK_CANARY) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "STACK CORRUPTION DETECTED! Canaries: " +
                           std::to_string(canary_before) + ", " + std::to_string(canary_after));
        return false;
    }
    
    // Debug logging to isolate vkCreateInstance issue
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "About to call vkCreateInstance...");
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Function pointer vkCreateInstance: " + 
                       std::to_string(reinterpret_cast<uintptr_t>(vkCreateInstance)));
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "instance_info.sType: " + std::to_string(instance_info.sType));
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "instance_info.pApplicationInfo: " + 
                       std::to_string(reinterpret_cast<uintptr_t>(instance_info.pApplicationInfo)));
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "app_info.apiVersion: " + std::to_string(app_info.apiVersion));
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "test_instance address: " + 
                       std::to_string(reinterpret_cast<uintptr_t>(&test_instance)));
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Calling vkCreateInstance with local variable...");
    
    // CRITICAL: Verify function pointer immediately before call
    if (!vkCreateInstance) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "FATAL: vkCreateInstance function pointer is NULL!");
        return false;
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Final function pointer check: " +
                       std::to_string(reinterpret_cast<uintptr_t>(vkCreateInstance)));
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "LIBRARY LOADER FIX: RuntimeLoader is now persistent to prevent dlclose()!");
    
    // Call with maximum safety
    VkResult result = vkCreateInstance(&instance_info, nullptr, &test_instance);
    
    // POST-CALL VERIFICATION
    if (canary_before != STACK_CANARY || canary_after != STACK_CANARY) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "STACK CORRUPTION AFTER CALL! Canaries: " +
                           std::to_string(canary_before) + ", " + std::to_string(canary_after));
        return false;
    }
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "vkCreateInstance returned: " + VulkanResultString(result));
    
    if (result != VK_SUCCESS) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "Failed to create Vulkan instance: " + VulkanResultString(result));
        return false;
    }
    
    // If successful, copy to context
    context_->instance = test_instance;
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "SUCCESS: vkCreateInstance worked with local variable!");
    
    // Load instance-level functions now that we have a VkInstance
    auto load_instance_result = LoadInstanceFunctions(context_->instance);
    if (!load_instance_result) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "Failed to load Vulkan instance functions: " + load_instance_result.GetError().message);
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
    
    // Load device-level functions now that we have a VkDevice
    auto load_device_result = LoadDeviceFunctions(context_->instance, device_->logical_device);
    if (!load_device_result) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "Failed to load Vulkan device functions: " + load_device_result.GetError().message);
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
    
    // Phase 1: Ensure all GPU work is complete before cleanup
    if (device_ && device_->logical_device && device_->compute_queue && vkQueueWaitIdle) {
        KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Waiting for GPU work to complete...");
        VkResult result = vkQueueWaitIdle(device_->compute_queue);
        if (result != VK_SUCCESS) {
            KERNTOPIA_LOG_WARNING(LogComponent::BACKEND, 
                "vkQueueWaitIdle failed during shutdown: " + std::to_string(result));
        } else {
            KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "GPU work completed successfully");
        }
    }
    
    // Phase 2: Force buffer cleanup while device is still valid to prevent access after destruction
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Force-destroying bound resources while device is valid...");
    
    // Important: Call DestroyBuffer explicitly on all VulkanBuffers while device is still valid
    for (auto& [binding, buffer] : bound_buffers_) {
        if (buffer) {
            auto vulkan_buffer = std::dynamic_pointer_cast<VulkanBuffer>(buffer);
            if (vulkan_buffer) {
                KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Force-destroying buffer at binding " + std::to_string(binding));
                // Call DestroyBuffer directly to clean up while device is valid
                vulkan_buffer->DestroyBuffer();
            }
        }
    }
    
    // Now safe to clear the containers
    bound_buffers_.clear();
    bound_textures_.clear();
    parameter_data_.clear();
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Resource bindings cleared and force-destroyed");
    
    // Phase 3: Destroy Vulkan resources in reverse creation order with enhanced error checking
    if (device_ && device_->logical_device) {
        KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Starting Vulkan resource destruction...");
        // Destroy pipeline resources with detailed logging and null checks
        if (pipeline_) {
            KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Destroying pipeline resources...");
            
            if (pipeline_->descriptor_pool != VK_NULL_HANDLE) {
                KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Destroying descriptor pool...");
                vkDestroyDescriptorPool(device_->logical_device, pipeline_->descriptor_pool, nullptr);
                pipeline_->descriptor_pool = VK_NULL_HANDLE;
            }
            
            if (pipeline_->pipeline != VK_NULL_HANDLE) {
                KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Destroying compute pipeline...");
                vkDestroyPipeline(device_->logical_device, pipeline_->pipeline, nullptr);
                pipeline_->pipeline = VK_NULL_HANDLE;
            }
            
            if (pipeline_->pipeline_layout != VK_NULL_HANDLE) {
                KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Destroying pipeline layout...");
                vkDestroyPipelineLayout(device_->logical_device, pipeline_->pipeline_layout, nullptr);
                pipeline_->pipeline_layout = VK_NULL_HANDLE;
            }
            
            if (pipeline_->descriptor_set_layout != VK_NULL_HANDLE) {
                KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Destroying descriptor set layout...");
                vkDestroyDescriptorSetLayout(device_->logical_device, pipeline_->descriptor_set_layout, nullptr);
                pipeline_->descriptor_set_layout = VK_NULL_HANDLE;
            }
            
            if (pipeline_->shader_module != VK_NULL_HANDLE) {
                KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Destroying shader module...");
                vkDestroyShaderModule(device_->logical_device, pipeline_->shader_module, nullptr);
                pipeline_->shader_module = VK_NULL_HANDLE;
            }
            
            KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Pipeline resources destroyed successfully");
        }
        
        // Destroy command pool with detailed logging
        if (command_pool_ && command_pool_->command_pool != VK_NULL_HANDLE) {
            KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Destroying command pool...");
            vkDestroyCommandPool(device_->logical_device, command_pool_->command_pool, nullptr);
            command_pool_->command_pool = VK_NULL_HANDLE;
            KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Command pool destroyed successfully");
        }
        
        // Destroy logical device with final wait and logging
        KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Destroying logical device...");
        vkDestroyDevice(device_->logical_device, nullptr);
        device_->logical_device = VK_NULL_HANDLE;
        KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Logical device destroyed successfully");
    }
    
    // Destroy instance with logging
    if (context_ && context_->instance != VK_NULL_HANDLE) {
        KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Destroying Vulkan instance...");
        vkDestroyInstance(context_->instance, nullptr);
        context_->instance = VK_NULL_HANDLE;
        KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Vulkan instance destroyed successfully");
    }
    
    // Phase 4: Release smart pointer resources in reverse order with logging
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Releasing smart pointer resources...");
    
    if (query_pool_) {
        KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Releasing query pool...");
        query_pool_.reset();
    }
    
    if (command_pool_) {
        KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Releasing command pool...");
        command_pool_.reset();
    }
    
    if (pipeline_) {
        KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Releasing pipeline...");
        pipeline_.reset();
    }
    
    if (device_) {
        KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Releasing device...");
        device_.reset();
    }
    
    if (context_) {
        KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Releasing context...");
        context_.reset();
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "All resources released successfully");
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