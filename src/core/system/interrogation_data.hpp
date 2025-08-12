#pragma once

#include "../common/error_handling.hpp"
#include "../backend/ikernel_runner.hpp"

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace kerntopia {

/**
 * @brief Runtime capability flags for different runtime types
 */
struct RuntimeCapabilities {
    bool jit_compilation = false;           ///< Supports just-in-time compilation
    bool precompiled_kernels = false;      ///< Supports precompiled kernel loading
    bool memory_management = false;        ///< Supports GPU memory allocation
    bool device_enumeration = false;       ///< Can enumerate available devices
    bool performance_counters = false;     ///< Supports performance monitoring
    
    // Target-specific capabilities
    std::vector<std::string> supported_targets;   ///< Compilation targets (spirv, ptx, etc.)
    std::vector<std::string> supported_profiles;  ///< Shader profiles (glsl_450, sm_6_0, etc.)
    std::vector<std::string> supported_stages;    ///< Shader stages (compute, vertex, etc.)
};

/**
 * @brief Unified runtime information for any GPU/compute runtime
 * 
 * This structure provides consistent representation for CUDA, Vulkan, SLANG,
 * and future runtimes. Designed for easy serialization to Python pickle format.
 */
struct RuntimeInfo {
    // Basic availability
    bool available = false;                 ///< Runtime is available and functional
    std::string name;                       ///< Human-readable name (e.g., "CUDA", "Vulkan", "SLANG")
    std::string version;                    ///< Runtime version string
    std::string error_message;              ///< Error description if not available
    
    // File system information
    std::vector<std::string> library_paths;    ///< All detected library paths
    std::vector<std::string> executable_paths; ///< All detected executable paths
    std::string primary_library_path;          ///< Primary library path (if multiple found)
    std::string primary_executable_path;       ///< Primary executable path (if multiple found)
    
    // Metadata for audit trail
    uint64_t library_file_size = 0;        ///< Primary library file size
    uint64_t executable_file_size = 0;     ///< Primary executable file size
    std::string library_checksum;          ///< Library file checksum
    std::string executable_checksum;       ///< Executable file checksum
    std::string library_last_modified;     ///< Library modification timestamp
    std::string executable_last_modified;  ///< Executable modification timestamp
    
    // Runtime capabilities
    RuntimeCapabilities capabilities;       ///< What this runtime can do
    
    // Device information (populated for GPU runtimes)
    std::vector<DeviceInfo> devices;        ///< Available devices for this runtime
};

/**
 * @brief Complete system interrogation results
 * 
 * Contains all detected runtimes, devices, and system information.
 * Designed for serialization to Python pickle format for reproducible reports.
 */
struct SystemInfo {
    // Runtime information
    RuntimeInfo cuda_runtime;              ///< CUDA runtime detection results
    RuntimeInfo vulkan_runtime;            ///< Vulkan runtime detection results  
    RuntimeInfo slang_runtime;             ///< SLANG compiler/runtime detection results
    
    // System-wide information
    std::string timestamp;                 ///< When interrogation was performed
    std::string hostname;                  ///< System hostname
    std::string os_version;                ///< Operating system version
    std::string architecture;              ///< CPU architecture (x86_64, arm64, etc.)
    
    // Build information
    std::string kerntopia_version;         ///< Kerntopia version
    std::string build_timestamp;           ///< When Kerntopia was built
    std::string cmake_version;             ///< CMake version used for build
    
    // Convenience methods for checking availability
    bool HasCuda() const { return cuda_runtime.available; }
    bool HasVulkan() const { return vulkan_runtime.available; }
    bool HasSlang() const { return slang_runtime.available; }
    
    std::vector<std::string> GetAvailableRuntimes() const {
        std::vector<std::string> runtimes;
        if (HasCuda()) runtimes.push_back("CUDA");
        if (HasVulkan()) runtimes.push_back("Vulkan");
        if (HasSlang()) runtimes.push_back("SLANG");
        return runtimes;
    }
};

/**
 * @brief Runtime type enumeration for consistent handling
 */
enum class RuntimeType {
    CUDA,
    VULKAN,
    SLANG,
    CPU  // Software-only runtime
};

/**
 * @brief Runtime utility functions
 */
namespace runtime_utils {
    
    /**
     * @brief Convert runtime type to string
     */
    std::string ToString(RuntimeType runtime);
    
    /**
     * @brief Parse runtime type from string
     */
    Result<RuntimeType> FromString(const std::string& name);
    
    /**
     * @brief Get all supported runtime types
     */
    std::vector<RuntimeType> GetAllRuntimeTypes();
}

} // namespace kerntopia