#pragma once

#include "ikernel_runner.hpp"
#include "runtime_loader.hpp"
#include "../common/error_handling.hpp"

#include <memory>
#include <vector>
#include <map>
#include <string>
#include <mutex>

namespace kerntopia {

/**
 * @brief Backend availability information
 */
struct BackendInfo {
    Backend type;                               ///< Backend type
    std::string name;                           ///< Human-readable name
    bool available = false;                     ///< Available on this system
    std::string version;                        ///< Backend version
    std::string library_path;                   ///< Path to runtime library
    std::vector<std::string> library_paths;     ///< All detected library paths
    std::string error_message;                  ///< Error if not available
    
    // Runtime information
    bool is_primary = true;                     ///< Primary runtime if multiple found
    std::string checksum;                       ///< Library file checksum
    uint64_t file_size = 0;                     ///< Library file size
    std::string last_modified;                  ///< File modification timestamp
};

/**
 * @brief Central factory for creating GPU backend instances with dynamic runtime loading
 * 
 * The BackendFactory provides a unified interface for detecting and creating GPU backend
 * instances. It handles dynamic loading of runtime libraries (CUDA, Vulkan) without
 * requiring static linking, enabling graceful degradation when backends are unavailable.
 * 
 * Key features:
 * - Dynamic runtime library detection and loading
 * - Comprehensive system interrogation 
 * - Duplicate runtime detection with primary/secondary identification
 * - Complete audit trail with checksums and version information
 * - Cross-platform compatibility (Windows/Linux)
 */
class BackendFactory {
public:
    /**
     * @brief Initialize factory and detect available backends
     * 
     * Scans system for available GPU runtime libraries and initializes
     * backend detection. This should be called once at application startup.
     * 
     * @return Success result
     */
    static Result<void> Initialize();
    
    /**
     * @brief Shutdown factory and cleanup resources
     * 
     * Unloads dynamic libraries and cleans up backend resources.
     * Should be called at application shutdown.
     */
    static void Shutdown();
    
    /**
     * @brief Get list of all available backends on this system
     * 
     * @return Vector of available backend types
     */
    static std::vector<Backend> GetAvailableBackends();
    
    /**
     * @brief Get detailed information about all backends
     * 
     * @return Map of backend type to detailed information
     */
    static std::map<Backend, BackendInfo> GetBackendInfo();
    
    /**
     * @brief Get information about specific backend
     * 
     * @param backend Backend type to query
     * @return Backend information or error if not found
     */
    static Result<BackendInfo> GetBackendInfo(Backend backend);
    
    /**
     * @brief Check if specific backend is available
     * 
     * @param backend Backend type to check
     * @return True if backend is available and functional
     */
    static bool IsBackendAvailable(Backend backend);
    
    /**
     * @brief Enumerate devices for specific backend
     * 
     * @param backend Backend type
     * @return List of available devices or error
     */
    static Result<std::vector<DeviceInfo>> GetDevices(Backend backend);
    
    /**
     * @brief Create kernel runner for specified backend and device
     * 
     * @param backend Backend type
     * @param device_id Device index (0-based)
     * @return Kernel runner instance or error
     */
    static Result<std::unique_ptr<IKernelRunner>> CreateRunner(Backend backend, int device_id = 0);
    
    /**
     * @brief Get factory instance for specific backend
     * 
     * @param backend Backend type
     * @return Factory instance or error
     */
    static Result<std::shared_ptr<IKernelRunnerFactory>> GetFactory(Backend backend);
    
    /**
     * @brief Register custom backend factory
     * 
     * Allows registration of custom backend implementations for testing
     * or future backend support.
     * 
     * @param backend Backend type
     * @param factory Factory instance
     */
    static void RegisterFactory(Backend backend, std::shared_ptr<IKernelRunnerFactory> factory);
    
    /**
     * @brief Force refresh of backend detection
     * 
     * Re-scans system for available backends. Useful if runtime libraries
     * are installed after application startup.
     * 
     * @return Success result
     */
    static Result<void> RefreshBackends();
    
    /**
     * @brief Get system-wide runtime information for debugging
     * 
     * @return Detailed system information including all detected runtimes
     */
    static std::string GetSystemInfo();
    
    /**
     * @brief Validate backend functionality
     * 
     * Performs basic validation to ensure backend can create devices and
     * execute simple operations.
     * 
     * @param backend Backend to validate
     * @return Validation result with details
     */
    static Result<void> ValidateBackend(Backend backend);

public:
    ~BackendFactory() = default;
    
private:
    BackendFactory() = default;
    
    // Singleton instance management
    static BackendFactory& GetInstance();
    static std::unique_ptr<BackendFactory> instance_;
    static std::mutex instance_mutex_;
    
    // Backend detection and initialization
    Result<void> InitializeImpl();
    void ShutdownImpl();
    Result<void> DetectBackends();
    Result<BackendInfo> DetectBackend(Backend backend);
    
    // Backend-specific detection methods
    Result<BackendInfo> DetectCudaBackend();
    Result<BackendInfo> DetectVulkanBackend();
    Result<BackendInfo> DetectCpuBackend();
    
    // Internal factory methods
    Result<std::shared_ptr<IKernelRunnerFactory>> GetFactoryImpl(Backend backend);
    Result<std::shared_ptr<IKernelRunnerFactory>> CreateCudaFactory();
    Result<std::shared_ptr<IKernelRunnerFactory>> CreateVulkanFactory();
    Result<std::shared_ptr<IKernelRunnerFactory>> CreateCpuFactory();
    
    // Runtime library management
    std::unique_ptr<RuntimeLoader> runtime_loader_;
    
    // Backend registry
    std::map<Backend, BackendInfo> backend_info_;
    std::map<Backend, std::shared_ptr<IKernelRunnerFactory>> factories_;
    
    // Thread safety
    mutable std::mutex mutex_;
    bool initialized_ = false;
};

/**
 * @brief Backend enumeration utilities
 */
namespace backend_utils {
    
    /**
     * @brief Convert backend enum to string
     * 
     * @param backend Backend type
     * @return Backend name string
     */
    std::string ToString(Backend backend);
    
    /**
     * @brief Parse backend from string
     * 
     * @param name Backend name string
     * @return Backend type or error
     */
    Result<Backend> FromString(const std::string& name);
    
    /**
     * @brief Get all supported backend types
     * 
     * @return Vector of all backend types
     */
    std::vector<Backend> GetAllBackends();
    
    /**
     * @brief Get default backend preference order
     * 
     * Returns backends in order of preference for automatic selection.
     * 
     * @return Vector of backends in preference order
     */
    std::vector<Backend> GetDefaultPreferenceOrder();
    
    /**
     * @brief Check if backend requires specific hardware
     * 
     * @param backend Backend type
     * @return True if backend requires specific GPU hardware
     */
    bool RequiresSpecificHardware(Backend backend);
    
    /**
     * @brief Get minimum system requirements for backend
     * 
     * @param backend Backend type
     * @return Requirements description string
     */
    std::string GetMinimumRequirements(Backend backend);
}

} // namespace kerntopia