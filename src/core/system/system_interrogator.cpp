#include "system_interrogator.hpp"
#include "device_info.hpp"
#include "../common/logger.hpp"
#include "../backend/runtime_loader.hpp"

#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace kerntopia {

// Static member definitions
// Note: RuntimeLoader is now a singleton, no static member needed
std::unique_ptr<SystemInfo> SystemInterrogator::cached_system_info_ = nullptr;
bool SystemInterrogator::cache_valid_ = false;

Result<SystemInfo> SystemInterrogator::GetSystemInfo() {
    if (cache_valid_ && cached_system_info_) {
        KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "SystemInterrogator: Using cached system info");
        SystemInfo info_copy = *cached_system_info_;
        return KERNTOPIA_SUCCESS(info_copy);
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "SystemInterrogator: Cache miss - performing full system interrogation");
    
    // Use singleton RuntimeLoader
    RuntimeLoader& runtime_loader = RuntimeLoader::GetInstance();
    
    SystemInfo info;
    
    // Collect system metadata
    CollectSystemMetadata(info);
    CollectBuildMetadata(info);
    
    // Detect all runtimes
    info.cuda_runtime = DetectCudaRuntime();
    info.vulkan_runtime = DetectVulkanRuntime();
    info.slang_runtime = DetectSlangRuntime();
    
    // Cache results
    cached_system_info_ = std::make_unique<SystemInfo>(info);
    cache_valid_ = true;
    
    return KERNTOPIA_SUCCESS(info);
}

Result<RuntimeInfo> SystemInterrogator::GetRuntimeInfo(RuntimeType runtime) {
    // Use cached system info instead of direct detection to avoid redundant calls
    auto system_info_result = GetSystemInfo();
    if (!system_info_result) {
        return KERNTOPIA_RESULT_ERROR(RuntimeInfo, ErrorCategory::SYSTEM, ErrorCode::SYSTEM_INTERROGATION_FAILED,
                                     "Failed to get system information");
    }
    
    const SystemInfo& info = system_info_result.GetValue();
    switch (runtime) {
        case RuntimeType::CUDA:
            return KERNTOPIA_SUCCESS(info.cuda_runtime);
        case RuntimeType::VULKAN:
            return KERNTOPIA_SUCCESS(info.vulkan_runtime);
        case RuntimeType::SLANG:
            return KERNTOPIA_SUCCESS(info.slang_runtime);
        case RuntimeType::CPU:
            {
                RuntimeInfo cpu_info;
                cpu_info.available = true;
                cpu_info.name = "CPU (Software)";
                cpu_info.version = "1.0.0";
                cpu_info.primary_library_path = "built-in";
                cpu_info.capabilities.precompiled_kernels = true;
                return KERNTOPIA_SUCCESS(cpu_info);
            }
        default:
            return KERNTOPIA_RESULT_ERROR(RuntimeInfo, ErrorCategory::VALIDATION,
                                         ErrorCode::INVALID_ARGUMENT,
                                         "Unknown runtime type");
    }
}

bool SystemInterrogator::IsRuntimeAvailable(RuntimeType runtime) {
    // Use cached system info instead of direct detection to avoid redundant calls
    auto system_info_result = GetSystemInfo();
    if (!system_info_result) {
        return false;
    }
    
    const SystemInfo& info = system_info_result.GetValue();
    switch (runtime) {
        case RuntimeType::CUDA:
            return info.cuda_runtime.available;
        case RuntimeType::VULKAN:
            return info.vulkan_runtime.available;
        case RuntimeType::SLANG:
            return info.slang_runtime.available;
        default:
            return false;
    }
}

Result<void> SystemInterrogator::RefreshRuntimes() {
    cache_valid_ = false;
    cached_system_info_.reset();
    return KERNTOPIA_VOID_SUCCESS();
}

RuntimeInfo SystemInterrogator::DetectCudaRuntime() {
    RuntimeInfo info;
    info.name = "CUDA";
    
    // Use singleton RuntimeLoader
    RuntimeLoader& runtime_loader = RuntimeLoader::GetInstance();
    
    // STEP 1: RuntimeLoader dynamic search FIRST (respects LD_LIBRARY_PATH, consistent with Vulkan)
    // Target only CUDA driver library, exclude SDK development libraries
    std::vector<std::string> cuda_driver_patterns = {"libcuda.so"};
    auto scan_result = runtime_loader.ScanForLibraries(cuda_driver_patterns);
    
    std::string found_driver_path;
    LibraryInfo driver_info;
    
    if (scan_result && !scan_result->empty()) {
        auto& libraries = *scan_result;
        
        // Should only find libcuda.so since we're specifically searching for it
        if (!libraries.empty()) {
            found_driver_path = libraries.begin()->second.full_path;
            driver_info = libraries.begin()->second;
            KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "Found CUDA driver via RuntimeLoader: " + found_driver_path);
        }
    }
    
    // STEP 2: Hardcoded fallback paths SECOND (WSL + standard locations)
    if (found_driver_path.empty()) {
        std::vector<std::string> fallback_paths = {
            // WSL-specific driver paths (Windows Subsystem for Linux)
            "/usr/lib/wsl/lib/libcuda.so.1",
            "/usr/lib/wsl/lib/libcuda.so",
            "/usr/lib/wsl/drivers/nvlti.inf_amd64_ebc0400e7490ee31/libcuda.so.1.1",
            
            // Standard Linux driver installation paths
            "/usr/lib/x86_64-linux-gnu/libcuda.so.1",
            "/usr/lib/x86_64-linux-gnu/libcuda.so",
            "/usr/lib64/libcuda.so.1", 
            "/usr/lib64/libcuda.so",
            "/usr/lib/libcuda.so.1",
            "/usr/lib/libcuda.so",
            "/usr/local/cuda/lib64/libcuda.so.1",
            "/usr/local/cuda/lib64/libcuda.so"
        };
        
        for (const std::string& lib_path : fallback_paths) {
            if (access(lib_path.c_str(), F_OK) == 0) {
                found_driver_path = lib_path;
                
                // Collect file metadata
                driver_info.full_path = lib_path;
                driver_info.name = lib_path.substr(lib_path.find_last_of('/') + 1);
                
                struct stat stat_buf;
                if (stat(lib_path.c_str(), &stat_buf) == 0) {
                    driver_info.file_size = stat_buf.st_size;
                    
                    char time_str[100];
                    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", 
                            localtime(&stat_buf.st_mtime));
                    driver_info.last_modified = time_str;
                    
                    driver_info.checksum = std::to_string(stat_buf.st_size) + "_" + 
                                         std::to_string(stat_buf.st_mtime);
                }
                
                KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "Found CUDA driver via fallback path: " + lib_path);
                break;
            }
        }
    }
    
    // Also check for CUDA runtime (cudart) for additional context
    std::vector<std::string> cudart_patterns = {"cudart"};
    auto cudart_scan_result = runtime_loader.ScanForLibraries(cudart_patterns);
    std::string found_runtime_path;
    
    if (cudart_scan_result && !cudart_scan_result->empty()) {
        auto& cudart_libraries = *cudart_scan_result;
        if (!cudart_libraries.empty()) {
            found_runtime_path = cudart_libraries.begin()->second.full_path;
            KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "Found CUDA runtime: " + found_runtime_path);
        }
    }
    
    // Determine availability based on what we found
    if (!found_driver_path.empty()) {
        info.available = true;
        info.primary_library_path = found_driver_path;
        info.library_file_size = driver_info.file_size;
        info.library_checksum = driver_info.checksum;
        info.library_last_modified = driver_info.last_modified;
        
        // Collect all CUDA library paths
        info.library_paths.push_back(found_driver_path);
        if (!found_runtime_path.empty()) {
            info.library_paths.push_back(found_runtime_path);
        }
        
        // Set version (simplified)
        info.version = "CUDA Driver (Dynamic Detection)";
        
        // Set CUDA capabilities
        info.capabilities.jit_compilation = true;  // Driver API supports PTX JIT
        info.capabilities.precompiled_kernels = true;
        info.capabilities.memory_management = true;
        info.capabilities.device_enumeration = true;
        info.capabilities.performance_counters = true;
        info.capabilities.supported_targets = {"ptx", "cubin"};
        info.capabilities.supported_profiles = {"cuda_sm_6_0", "cuda_sm_7_0", "cuda_sm_7_5", "cuda_sm_8_0", "cuda_sm_8_9"};
        info.capabilities.supported_stages = {"compute"};
        
        // Device enumeration is left as placeholder in SystemInterrogator
        // Full device enumeration is handled by CUDA backend for proper separation
        info.devices = EnumerateCudaDevices(info);
        
        KERNTOPIA_LOG_INFO(LogComponent::SYSTEM, "CUDA driver library detected: " + found_driver_path);
        
    } else {
        info.available = false;
        info.error_message = "CUDA driver library (libcuda.so) not found via dynamic search or standard paths";
        
        // Provide helpful context if only runtime was found
        if (!found_runtime_path.empty()) {
            info.error_message += " (found runtime library at " + found_runtime_path + 
                                 " but need driver library for PTX JIT)";
        }
        
        KERNTOPIA_LOG_WARNING(LogComponent::SYSTEM, "CUDA detection failed: " + info.error_message);
    }
    
    return info;
}

RuntimeInfo SystemInterrogator::DetectVulkanRuntime() {
    RuntimeInfo info;
    info.name = "Vulkan";
    
    // Use singleton RuntimeLoader
    RuntimeLoader& runtime_loader = RuntimeLoader::GetInstance();
    
    // Search for Vulkan libraries using existing pattern
    std::vector<std::string> vulkan_patterns = {"vulkan"};
    auto scan_result = runtime_loader.ScanForLibraries(vulkan_patterns);
    
    if (!scan_result || scan_result->empty()) {
        info.available = false;
        info.error_message = "Vulkan libraries not found";
        return info;
    }
    
    // Process detected libraries
    auto& libraries = *scan_result;
    if (!libraries.empty()) {
        auto& first_lib = libraries.begin()->second;
        info.available = true;
        info.primary_library_path = first_lib.full_path;
        info.version = first_lib.version;
        info.library_checksum = first_lib.checksum;
        info.library_file_size = first_lib.file_size;
        info.library_last_modified = first_lib.last_modified;
        
        // Collect all library paths
        for (const auto& [name, lib_info] : libraries) {
            info.library_paths.push_back(lib_info.full_path);
        }
        
        // Set Vulkan capabilities
        info.capabilities.jit_compilation = false;  // Vulkan uses precompiled SPIR-V
        info.capabilities.precompiled_kernels = true;
        info.capabilities.memory_management = true;
        info.capabilities.device_enumeration = true;
        info.capabilities.performance_counters = true;
        info.capabilities.supported_targets = {"spirv"};
        info.capabilities.supported_profiles = {"glsl_450", "glsl_460"};
        info.capabilities.supported_stages = {"compute", "vertex", "fragment"};
        
        // Enumerate devices
        info.devices = EnumerateVulkanDevices(info);
    }
    
    return info;
}

RuntimeInfo SystemInterrogator::DetectSlangRuntime() {
    RuntimeInfo info;
    info.name = "SLANG";
    
    // Use singleton RuntimeLoader
    RuntimeLoader& runtime_loader = RuntimeLoader::GetInstance();
    
    // **ENHANCED DETECTION**: Both executable AND shared library
    
    // 1. Detect slangc executable (compiler)
    std::vector<std::string> search_paths;
    
    // Add PATH directories
    const char* path_env = std::getenv("PATH");
    if (path_env) {
        std::string path_str(path_env);
        size_t start = 0;
        size_t end = path_str.find(':');
        
        while (end != std::string::npos) {
            search_paths.push_back(path_str.substr(start, end - start));
            start = end + 1;
            end = path_str.find(':', start);
        }
        search_paths.push_back(path_str.substr(start));
    }
    
    // Add FetchContent location from build
    search_paths.push_back("build/_deps/slang-src/bin");
    search_paths.push_back("_deps/slang-src/bin");
    
    // Search for slangc executable
    std::string slangc_path;
    for (const auto& search_path : search_paths) {
        std::string candidate = search_path + "/slangc";
        
        if (access(candidate.c_str(), F_OK) == 0 && access(candidate.c_str(), X_OK) == 0) {
            slangc_path = candidate;
            break;
        }
    }
    
    // 2. Detect SLANG shared library (runtime) - look for actual libslang, not glslang
    std::vector<std::string> slang_lib_patterns = {"libslang"};
    auto lib_scan_result = runtime_loader.ScanForLibraries(slang_lib_patterns);
    
    // Also check build directory for libslang.so
    if (!lib_scan_result || lib_scan_result->empty()) {
        // Check FetchContent SLANG library location
        std::vector<std::string> slang_build_paths = {
            "build/_deps/slang-src/lib/libslang.so",
            "_deps/slang-src/lib/libslang.so",
            "build/_deps/slang-src/lib/libslang.a",
            "_deps/slang-src/lib/libslang.a"
        };
        
        for (const auto& lib_path : slang_build_paths) {
            if (access(lib_path.c_str(), F_OK) == 0) {
                // Found SLANG library - create a mock scan result
                LibraryInfo lib_info;
                lib_info.full_path = lib_path;
                lib_info.name = lib_path.substr(lib_path.find_last_of('/') + 1);
                
                struct stat stat_buf;
                if (stat(lib_path.c_str(), &stat_buf) == 0) {
                    lib_info.file_size = stat_buf.st_size;
                    
                    char time_str[100];
                    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", 
                            localtime(&stat_buf.st_mtime));
                    lib_info.last_modified = time_str;
                    
                    lib_info.checksum = std::to_string(stat_buf.st_size) + "_" + 
                                       std::to_string(stat_buf.st_mtime);
                }
                
                // Create mock scan result
                std::map<std::string, LibraryInfo> mock_result;
                mock_result["libslang"] = lib_info;
                lib_scan_result = Result<std::map<std::string, LibraryInfo>>::Success(mock_result);
                break;
            }
        }
    }
    
    // Check if we have either executable or library
    bool has_executable = !slangc_path.empty();
    bool has_library = lib_scan_result && !lib_scan_result->empty();
    
    if (!has_executable && !has_library) {
        info.available = false;
        info.error_message = "SLANG compiler and runtime library not found";
        return info;
    }
    
    info.available = true;
    
    // Process executable information
    if (has_executable) {
        info.primary_executable_path = slangc_path;
        info.executable_paths.push_back(slangc_path);
        
        // Get executable metadata
        CollectFileMetadata(slangc_path, info.executable_file_size, 
                           info.executable_checksum, info.executable_last_modified);
        
        // Get version from slangc -h (since --version is not supported)
        FILE* version_pipe = popen((slangc_path + " -h 2>&1").c_str(), "r");
        if (version_pipe) {
            char buffer[256];
            std::string version_output;
            
            while (fgets(buffer, sizeof(buffer), version_pipe)) {
                version_output += buffer;
            }
            pclose(version_pipe);
            
            // Use known version from FetchContent
            if (version_output.find("slang") != std::string::npos) {
                info.version = "2025.14.3";
            } else {
                info.version = "unknown";
            }
        }
        
        // Get supported targets from slangc -h target
        FILE* targets_pipe = popen((slangc_path + " -h target 2>&1").c_str(), "r");
        if (targets_pipe) {
            char buffer[256];
            std::string targets_output;
            
            while (fgets(buffer, sizeof(buffer), targets_pipe)) {
                targets_output += buffer;
            }
            pclose(targets_pipe);
            
            // Parse targets from help output
            if (targets_output.find("spirv") != std::string::npos) {
                info.capabilities.supported_targets.push_back("spirv");
            }
            if (targets_output.find("ptx") != std::string::npos) {
                info.capabilities.supported_targets.push_back("ptx");
            }
            if (targets_output.find("dxil") != std::string::npos) {
                info.capabilities.supported_targets.push_back("dxil");
            }
            if (targets_output.find("glsl") != std::string::npos) {
                info.capabilities.supported_targets.push_back("glsl");
            }
        }
        
        // Get supported profiles
        FILE* profiles_pipe = popen((slangc_path + " -h profile 2>&1").c_str(), "r");
        if (profiles_pipe) {
            char buffer[256];
            std::string profiles_output;
            
            while (fgets(buffer, sizeof(buffer), profiles_pipe)) {
                profiles_output += buffer;
            }
            pclose(profiles_pipe);
            
            if (profiles_output.find("glsl_450") != std::string::npos) {
                info.capabilities.supported_profiles.push_back("glsl_450");
            }
            if (profiles_output.find("sm_") != std::string::npos) {
                info.capabilities.supported_profiles.push_back("sm_6_0");
                info.capabilities.supported_profiles.push_back("sm_6_5");
            }
        }
    }
    
    // Process library information  
    if (has_library) {
        auto& libraries = *lib_scan_result;
        if (!libraries.empty()) {
            auto& first_lib = libraries.begin()->second;
            info.primary_library_path = first_lib.full_path;
            info.library_file_size = first_lib.file_size;
            info.library_last_modified = first_lib.last_modified;
            info.library_checksum = first_lib.checksum;
            
            for (const auto& [name, lib_info] : libraries) {
                info.library_paths.push_back(lib_info.full_path);
            }
        }
    }
    
    // Set SLANG capabilities
    info.capabilities.jit_compilation = has_library;  // JIT requires runtime library
    info.capabilities.precompiled_kernels = has_executable;  // Precompile requires compiler
    info.capabilities.memory_management = false;  // SLANG doesn't handle GPU memory directly
    info.capabilities.device_enumeration = false;  // SLANG is target-agnostic
    info.capabilities.performance_counters = false;
    info.capabilities.supported_stages = {"compute"};  // Focus on compute shaders
    
    // Update error message if partially available
    if (has_executable && !has_library) {
        info.error_message = "SLANG compiler available but runtime library not found - JIT compilation unavailable";
    } else if (!has_executable && has_library) {
        info.error_message = "SLANG runtime library available but compiler not found - precompilation unavailable";
    }
    
    return info;
}

void SystemInterrogator::CollectSystemMetadata(SystemInfo& info) {
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    info.timestamp = ss.str();
    
    // Get hostname
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        info.hostname = hostname;
    } else {
        info.hostname = "unknown";
    }
    
    // Get OS version (simplified)
    info.os_version = "Linux";  // Could be enhanced with uname() calls
    info.architecture = "x86_64";  // Could be enhanced with platform detection
}

void SystemInterrogator::CollectBuildMetadata(SystemInfo& info) {
    info.kerntopia_version = "0.1.0";
    info.build_timestamp = __DATE__ " " __TIME__;
    info.cmake_version = "3.28+";  // Could be populated from CMake variables
}

std::vector<DeviceInfo> SystemInterrogator::EnumerateCudaDevices(const RuntimeInfo& cuda_info) {
    // Simplified device enumeration - would use actual CUDA API in full implementation
    std::vector<DeviceInfo> devices;
    if (cuda_info.available) {
        // Placeholder device info
        DeviceInfo device;
        device.name = "CUDA Device (Detection Pending)";
        device.backend_type = Backend::CUDA;
        device.total_memory_bytes = 0;  // Would query actual device
        devices.push_back(device);
    }
    return devices;
}

std::vector<DeviceInfo> SystemInterrogator::EnumerateVulkanDevices(const RuntimeInfo& vulkan_info) {
    // Simplified device enumeration - would use actual Vulkan API in full implementation
    std::vector<DeviceInfo> devices;
    if (vulkan_info.available) {
        // Placeholder device info
        DeviceInfo device;
        device.name = "Vulkan Device (Detection Pending)";
        device.backend_type = Backend::VULKAN;
        device.total_memory_bytes = 0;  // Would query actual device
        devices.push_back(device);
    }
    return devices;
}

void SystemInterrogator::CollectFileMetadata(const std::string& file_path, 
                                            uint64_t& file_size, 
                                            std::string& checksum, 
                                            std::string& last_modified) {
    struct stat stat_buf;
    if (stat(file_path.c_str(), &stat_buf) == 0) {
        file_size = stat_buf.st_size;
        
        // Format last modified time
        char time_str[100];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", 
                localtime(&stat_buf.st_mtime));
        last_modified = time_str;
        
        // Calculate simple checksum (could use proper SHA256)
        checksum = std::to_string(stat_buf.st_size) + "_" + 
                  std::to_string(stat_buf.st_mtime);
    }
}

// Runtime utility functions implementation
namespace runtime_utils {

std::string ToString(RuntimeType runtime) {
    switch (runtime) {
        case RuntimeType::CUDA:   return "CUDA";
        case RuntimeType::VULKAN: return "Vulkan";
        case RuntimeType::SLANG:  return "SLANG";
        case RuntimeType::CPU:    return "CPU";
        default:                  return "Unknown";
    }
}

Result<RuntimeType> FromString(const std::string& name) {
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    
    if (lower_name == "cuda") return KERNTOPIA_SUCCESS(RuntimeType::CUDA);
    if (lower_name == "vulkan") return KERNTOPIA_SUCCESS(RuntimeType::VULKAN);
    if (lower_name == "slang") return KERNTOPIA_SUCCESS(RuntimeType::SLANG);
    if (lower_name == "cpu") return KERNTOPIA_SUCCESS(RuntimeType::CPU);
    
    return KERNTOPIA_RESULT_ERROR(RuntimeType, ErrorCategory::VALIDATION,
                                 ErrorCode::INVALID_ARGUMENT,
                                 "Unknown runtime name: " + name);
}

std::vector<RuntimeType> GetAllRuntimeTypes() {
    return {RuntimeType::CUDA, RuntimeType::VULKAN, RuntimeType::SLANG, RuntimeType::CPU};
}

} // namespace runtime_utils

} // namespace kerntopia