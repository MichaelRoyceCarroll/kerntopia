#pragma once

#include "interrogation_data.hpp"
#include "device_info.hpp"
#include "../backend/runtime_loader.hpp"
#include "../common/error_handling.hpp"

#include <memory>
#include <string>

namespace kerntopia {

/**
 * @brief Unified system interrogation for all GPU/compute runtimes
 * 
 * The SystemInterrogator provides a centralized, consistent interface for detecting
 * and interrogating all supported runtimes (CUDA, Vulkan, SLANG). It replaces the
 * scattered detection logic previously embedded in BackendFactory.
 * 
 * Key features:
 * - Consistent detection patterns across all runtimes
 * - Unified data structures for easy serialization
 * - Graceful degradation when runtimes are unavailable
 * - Comprehensive audit trail with checksums and timestamps
 * - Designed for reuse across suite, standalone, and Python wrapper executables
 */
class SystemInterrogator {
public:
    /**
     * @brief Perform complete system interrogation
     * 
     * Detects all available runtimes, enumerates devices, and collects
     * comprehensive system information for reproducible reports.
     * 
     * @return Complete system information or error
     */
    static Result<SystemInfo> GetSystemInfo();
    
    /**
     * @brief Get information about specific runtime
     * 
     * @param runtime Runtime type to interrogate
     * @return Runtime information or error
     */
    static Result<RuntimeInfo> GetRuntimeInfo(RuntimeType runtime);
    
    /**
     * @brief Check if specific runtime is available
     * 
     * @param runtime Runtime type to check
     * @return True if runtime is available and functional
     */
    static bool IsRuntimeAvailable(RuntimeType runtime);
    
    /**
     * @brief Force refresh of runtime detection
     * 
     * Re-scans system for available runtimes. Useful if runtime libraries
     * are installed after application startup.
     * 
     * @return Success result
     */
    static Result<void> RefreshRuntimes();
    
    /**
     * @brief Get the selected Vulkan library path
     * 
     * Returns the path to the Vulkan library that was selected during
     * detection, allowing backends to use the same library.
     * 
     * @return Library path or error if Vulkan not available
     */
    static Result<std::string> GetVulkanLibraryPath();
    
    /**
     * @brief Get required Vulkan instance extensions
     * 
     * Returns the list of Vulkan instance extensions that should be
     * enabled for proper functionality.
     * 
     * @return List of extension names
     */
    static std::vector<std::string> GetVulkanInstanceExtensions();
    
    /**
     * @brief Validate that a Vulkan device ID is available
     * 
     * Checks if the specified device ID corresponds to a valid,
     * available Vulkan device.
     * 
     * @param device_id Device ID to validate
     * @return True if device is valid and available
     */
    static bool ValidateVulkanDevice(int device_id);
    
    /**
     * @brief Get the loaded Vulkan library handle
     * 
     * Returns the library handle that was loaded during Vulkan runtime detection.
     * This allows backends to use the same library instance for consistency.
     * 
     * @return Library handle or error if Vulkan not loaded
     */
    static Result<void*> GetVulkanLibraryHandle();

private:
    SystemInterrogator() = default;
    
    // Runtime-specific detection methods
    static RuntimeInfo DetectCudaRuntime();
    static RuntimeInfo DetectVulkanRuntime();
    static RuntimeInfo DetectSlangRuntime();
    
    // System information collection
    static void CollectSystemMetadata(SystemInfo& info);
    static void CollectBuildMetadata(SystemInfo& info);
    
    // Device enumeration helpers
    static std::vector<DeviceInfo> EnumerateCudaDevices(const RuntimeInfo& cuda_info);
    static std::vector<DeviceInfo> EnumerateVulkanDevices(const RuntimeInfo& vulkan_info);
    
    // File system helpers (reused from RuntimeLoader pattern)
    static void CollectFileMetadata(const std::string& file_path, 
                                   uint64_t& file_size, 
                                   std::string& checksum, 
                                   std::string& last_modified);
    
    // Note: RuntimeLoader is now a singleton, accessed via RuntimeLoader::GetInstance()
    
    // Cached results to avoid repeated detection
    static std::unique_ptr<SystemInfo> cached_system_info_;
    static bool cache_valid_;
    
    // Loaded library handles for compatibility layer
    static void* cached_vulkan_library_handle_;
};

} // namespace kerntopia