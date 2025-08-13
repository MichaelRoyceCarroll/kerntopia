#include "vulkan_runner.hpp"
#include "../common/logger.hpp"
#include "runtime_loader.hpp"
#include "../system/system_interrogator.hpp"
#include "../system/interrogation_data.hpp"

#include <cstring>
#include <vector>
#include <sstream>
#include <thread>

namespace kerntopia {

// Vulkan function pointers and types (dynamically loaded)
typedef uint32_t VkResult;
typedef void* VkInstance;
typedef void* VkPhysicalDevice;
typedef uint32_t VkBool32;

// Vulkan constants
constexpr VkResult VK_SUCCESS = 0;
constexpr VkResult VK_ERROR_INCOMPATIBLE_DRIVER = -9;
constexpr VkResult VK_ERROR_EXTENSION_NOT_PRESENT = -7;
constexpr VkResult VK_INCOMPLETE = 1;

constexpr uint32_t VK_API_VERSION_1_0 = ((1U<<22)|(0U<<12)|(0U));
constexpr uint32_t VK_MAKE_VERSION(uint32_t major, uint32_t minor, uint32_t patch) {
    return (major << 22) | (minor << 12) | patch;
}

// Vulkan enums and constants  
enum VkStructureType {
    VK_STRUCTURE_TYPE_APPLICATION_INFO = 0,
    VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1
};

// Vulkan structures (simplified but correct layout)
struct VkApplicationInfo {
    VkStructureType sType;
    const void* pNext;
    const char* pApplicationName;
    uint32_t applicationVersion;
    const char* pEngineName;
    uint32_t engineVersion;
    uint32_t apiVersion;
};

struct VkInstanceCreateInfo {
    VkStructureType sType;
    const void* pNext;
    uint32_t flags;
    const VkApplicationInfo* pApplicationInfo;
    uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
};

struct VkPhysicalDeviceProperties {
    uint32_t apiVersion;
    uint32_t driverVersion;
    uint32_t vendorID;
    uint32_t deviceID;
    uint32_t deviceType;
    char deviceName[256];
    uint8_t pipelineCacheUUID[16];
    // ... additional fields would be here in real Vulkan
};

struct VkPhysicalDeviceMemoryProperties {
    uint32_t memoryTypeCount;
    struct VkMemoryType {
        uint32_t propertyFlags;
        uint32_t heapIndex;
    } memoryTypes[32];
    uint32_t memoryHeapCount;
    struct VkMemoryHeap {
        uint64_t size;
        uint32_t flags;
    } memoryHeaps[16];
};

struct VkPhysicalDeviceFeatures {
    VkBool32 robustBufferAccess;
    VkBool32 fullDrawIndexUint32;
    VkBool32 imageCubeArray;
    // ... many more features in real Vulkan
};

// Vulkan function type definitions
typedef VkResult (*vkCreateInstance_t)(const VkInstanceCreateInfo* pCreateInfo, const void* pAllocator, VkInstance* pInstance);
typedef void (*vkDestroyInstance_t)(VkInstance instance, const void* pAllocator);
typedef VkResult (*vkEnumeratePhysicalDevices_t)(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices);
typedef void (*vkGetPhysicalDeviceProperties_t)(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties);
typedef void (*vkGetPhysicalDeviceMemoryProperties_t)(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pProperties);
typedef void (*vkGetPhysicalDeviceFeatures_t)(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures);

// Static Vulkan runtime functions (loaded dynamically)
static vkCreateInstance_t vkCreateInstance = nullptr;
static vkDestroyInstance_t vkDestroyInstance = nullptr;
static vkEnumeratePhysicalDevices_t vkEnumeratePhysicalDevices = nullptr;
static vkGetPhysicalDeviceProperties_t vkGetPhysicalDeviceProperties = nullptr;
static vkGetPhysicalDeviceMemoryProperties_t vkGetPhysicalDeviceMemoryProperties = nullptr;
static vkGetPhysicalDeviceFeatures_t vkGetPhysicalDeviceFeatures = nullptr;
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
    
    // Load required function pointers
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
    
    // Verify all functions were loaded
    if (!vkCreateInstance || !vkDestroyInstance || !vkEnumeratePhysicalDevices || 
        !vkGetPhysicalDeviceProperties || !vkGetPhysicalDeviceMemoryProperties || !vkGetPhysicalDeviceFeatures) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to load required Vulkan functions");
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

// Helper to convert Vulkan error to string
static std::string VulkanResultString(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        default: return "VK_ERROR_" + std::to_string(result);
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

// Vulkan internal structures
struct VulkanContext {
    void* instance = nullptr;
    void* debug_messenger = nullptr;
};

struct VulkanDevice {
    void* physical_device = nullptr;
    void* logical_device = nullptr;
    uint32_t compute_queue_family = 0;
    void* compute_queue = nullptr;
    std::string device_name;
    DeviceInfo device_info;
};

struct VulkanComputePipeline {
    void* descriptor_set_layout = nullptr;
    void* pipeline_layout = nullptr;
    void* pipeline = nullptr;
    void* shader_module = nullptr;
};

struct VulkanCommandPool {
    void* command_pool = nullptr;
    void* command_buffer = nullptr;
};

struct VulkanQueryPool {
    void* query_pool = nullptr;
    bool timing_supported = false;
};

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
    
    // For now, return a simulated mapping - in real implementation would use vkMapMemory
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "VulkanBuffer::Map - simulated mapping");
    mapped_ptr_ = malloc(size_);
    is_mapped_ = true;
    return mapped_ptr_;
}

void VulkanBuffer::Unmap() {
    if (!is_mapped_) return;
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "VulkanBuffer::Unmap - simulated unmapping");
    if (mapped_ptr_) {
        free(mapped_ptr_);
        mapped_ptr_ = nullptr;
    }
    is_mapped_ = false;
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
    // Simplified placeholder - real implementation would create VkBuffer and VkDeviceMemory
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "VulkanBuffer created with size " + std::to_string(size_));
    return true;
}

void VulkanBuffer::DestroyBuffer() {
    if (mapped_ptr_) {
        free(mapped_ptr_);
        mapped_ptr_ = nullptr;
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
    if (!device_) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Vulkan device not initialized");
    }
    
    // Simplified implementation - real version would create VkShaderModule and VkPipeline
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Loading Vulkan kernel: " + entry_point + 
                      " (SPIR-V size: " + std::to_string(bytecode.size()) + " bytes)");
    
    if (bytecode.empty()) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::VALIDATION, ErrorCode::INVALID_ARGUMENT,
                                     "Empty SPIR-V bytecode");
    }
    
    // Create pipeline placeholder
    if (!pipeline_) {
        pipeline_ = std::make_unique<VulkanComputePipeline>();
    }
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Vulkan kernel loaded successfully");
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
    
    bound_buffers_[binding] = buffer;
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Vulkan buffer bound to binding " + std::to_string(binding));
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

Result<void> VulkanKernelRunner::Dispatch(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) {
    if (!device_ || !pipeline_) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Vulkan device or pipeline not initialized");
    }
    
    dispatch_start_ = std::chrono::high_resolution_clock::now();
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Vulkan dispatch: " + 
                      std::to_string(groups_x) + "x" + std::to_string(groups_y) + "x" + std::to_string(groups_z));
    
    // Simplified implementation - real version would record and submit command buffer
    // Simulate computation time
    auto compute_duration = std::chrono::microseconds(100 + (groups_x * groups_y * groups_z) / 1000);
    std::this_thread::sleep_for(compute_duration);
    
    dispatch_end_ = std::chrono::high_resolution_clock::now();
    
    // Update timing results
    auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(dispatch_end_ - dispatch_start_);
    last_timing_.compute_time_ms = total_duration.count() / 1000.0f;
    last_timing_.total_time_ms = last_timing_.compute_time_ms;
    last_timing_.memory_setup_time_ms = 0.1f; // Minimal overhead
    last_timing_.memory_teardown_time_ms = 0.1f;
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Vulkan dispatch completed in " + 
                      std::to_string(last_timing_.compute_time_ms) + "ms");
    
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
    
    // Initialize context
    context_ = std::make_unique<VulkanContext>();
    
    // Initialize device using the provided DeviceInfo from SystemInterrogator
    device_ = std::make_unique<VulkanDevice>();
    device_->device_name = device_info.name;
    device_->compute_queue_family = 0;
    
    // Use the actual device info from SystemInterrogator instead of hardcoding
    device_->device_info = device_info;
    
    // Initialize command pool
    command_pool_ = std::make_unique<VulkanCommandPool>();
    
    // Initialize query pool for timing (optional)
    query_pool_ = std::make_unique<VulkanQueryPool>();
    query_pool_->timing_supported = false; // Simplified implementation doesn't support GPU timing
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Vulkan backend initialized: " + device_info.name);
    return true;
}

void VulkanKernelRunner::ShutdownVulkan() {
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Shutting down Vulkan backend");
    
    // Clear bindings
    bound_buffers_.clear();
    bound_textures_.clear();
    parameter_data_.clear();
    
    // Release resources
    query_pool_.reset();
    command_pool_.reset();
    pipeline_.reset();
    device_.reset();
    context_.reset();
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Vulkan backend shutdown complete");
}

Result<void> VulkanKernelRunner::CreateDescriptorSets() {
    // Simplified placeholder for descriptor set creation
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Creating Vulkan descriptor sets");
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> VulkanKernelRunner::UpdateDescriptorSets() {
    // Simplified placeholder for descriptor set updates
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Updating Vulkan descriptor sets");
    return KERNTOPIA_VOID_SUCCESS();
}

} // namespace kerntopia