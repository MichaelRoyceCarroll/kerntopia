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

// Include Vulkan headers for enhanced detection - ONLY if SDK is available
#ifdef KERNTOPIA_VULKAN_SDK_AVAILABLE
#include <vulkan/vulkan.h>
#endif

// Include CUDA headers for enhanced detection - ONLY if SDK is available
#ifdef KERNTOPIA_CUDA_SDK_AVAILABLE
#include <cuda.h>
#endif

namespace kerntopia {

// Static member definitions
// Note: RuntimeLoader is now a singleton, no static member needed
std::unique_ptr<SystemInfo> SystemInterrogator::cached_system_info_ = nullptr;
bool SystemInterrogator::cache_valid_ = false;
void* SystemInterrogator::cached_vulkan_library_handle_ = nullptr;

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

Result<std::string> SystemInterrogator::GetVulkanLibraryPath() {
    // Use cached system info to get the selected Vulkan library path
    auto system_info_result = GetSystemInfo();
    if (!system_info_result) {
        return KERNTOPIA_RESULT_ERROR(std::string, ErrorCategory::SYSTEM, ErrorCode::SYSTEM_INTERROGATION_FAILED,
                                     "Failed to get system information");
    }
    
    const SystemInfo& info = system_info_result.GetValue();
    if (!info.vulkan_runtime.available) {
        return KERNTOPIA_RESULT_ERROR(std::string, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Vulkan runtime not available");
    }
    
    if (info.vulkan_runtime.primary_library_path.empty()) {
        return KERNTOPIA_RESULT_ERROR(std::string, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "No Vulkan library path detected");
    }
    
    return KERNTOPIA_SUCCESS(info.vulkan_runtime.primary_library_path);
}

std::vector<std::string> SystemInterrogator::GetVulkanInstanceExtensions() {
    // For now, return minimal extensions for compute workloads
    // Could be enhanced based on detected capabilities
    std::vector<std::string> extensions;
    
    // No extensions required for basic compute workloads
    // Extensions like VK_KHR_surface, VK_KHR_swapchain only needed for graphics
    
    return extensions;
}

bool SystemInterrogator::ValidateVulkanDevice(int device_id) {
    // Use cached system info to validate device
    auto system_info_result = GetSystemInfo();
    if (!system_info_result) {
        return false;
    }
    
    const SystemInfo& info = system_info_result.GetValue();
    if (!info.vulkan_runtime.available || info.vulkan_runtime.devices.empty()) {
        return false;
    }
    
    // Check if device_id is within valid range
    return (device_id >= 0 && device_id < static_cast<int>(info.vulkan_runtime.devices.size()));
}

Result<void*> SystemInterrogator::GetVulkanLibraryHandle() {
    // Ensure we have detected Vulkan runtime and cached the library handle
    auto system_info_result = GetSystemInfo();
    if (!system_info_result) {
        return KERNTOPIA_RESULT_ERROR(void*, ErrorCategory::SYSTEM, ErrorCode::SYSTEM_INTERROGATION_FAILED,
                                     "Failed to get system information");
    }
    
    const SystemInfo& info = system_info_result.GetValue();
    if (!info.vulkan_runtime.available) {
        return KERNTOPIA_RESULT_ERROR(void*, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Vulkan runtime not available");
    }
    
    if (!cached_vulkan_library_handle_) {
        return KERNTOPIA_RESULT_ERROR(void*, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Vulkan library handle not cached");
    }
    
    return KERNTOPIA_SUCCESS(cached_vulkan_library_handle_);
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
    
    // PHASE 1: Set up ICD environment for Lavapipe (CPU Vulkan implementation)
    const char* vk_icd_filenames = std::getenv("VK_ICD_FILENAMES");
    if (!vk_icd_filenames) {
        KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "Setting VK_ICD_FILENAMES for Lavapipe CPU support");
        setenv("VK_ICD_FILENAMES", "/usr/share/vulkan/icd.d/lvp_icd.x86_64.json", 1);
    } else {
        KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "VK_ICD_FILENAMES already set: " + std::string(vk_icd_filenames));
    }
    
    // PHASE 2: Use sophisticated library selection (matching vulkan_runner.cpp priority)
    RuntimeLoader& runtime_loader = RuntimeLoader::GetInstance();
    std::vector<std::string> vulkan_candidates;
    
    // FIRST priority: System paths (these work best in WSL/Linux environments) 
    std::vector<std::string> system_paths = {
        "/usr/lib/x86_64-linux-gnu/libvulkan.so.1",
        "/usr/lib/x86_64-linux-gnu/libvulkan.so",
        "/usr/lib/libvulkan.so.1", 
        "/usr/lib/libvulkan.so"
    };
    
    // Add system paths first
    for (const std::string& path : system_paths) {
        vulkan_candidates.push_back(path);
    }
    
    // SECOND priority: SDK and custom paths via ScanForLibraries
    std::vector<std::string> vulkan_patterns = {"vulkan", "vulkan-1"};
    auto scan_result = runtime_loader.ScanForLibraries(vulkan_patterns);
    if (scan_result) {
        for (const auto& [name, lib_info] : *scan_result) {
            // Only add if not already in system paths
            if (std::find(vulkan_candidates.begin(), vulkan_candidates.end(), lib_info.full_path) == vulkan_candidates.end()) {
                vulkan_candidates.push_back(lib_info.full_path);
            }
        }
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "Vulkan library candidates found: " + std::to_string(vulkan_candidates.size()));
    for (size_t i = 0; i < vulkan_candidates.size(); ++i) {
        KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "  [" + std::to_string(i+1) + "] " + vulkan_candidates[i]);
    }
    
    // PHASE 3: Try to load first successfully loadable candidate
    std::string selected_library;
    void* library_handle = nullptr;
    
    for (const std::string& candidate : vulkan_candidates) {
        auto load_result = runtime_loader.LoadLibrary(candidate);
        if (load_result) {
            library_handle = *load_result;
            selected_library = candidate;
            KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "Selected Vulkan library: " + candidate);
            
            // Store the library handle for compatibility layer access
            cached_vulkan_library_handle_ = library_handle;
            
            break;
        } else {
            KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "Failed to load candidate: " + candidate);
        }
    }
    
    if (!library_handle || selected_library.empty()) {
        info.available = false;
        info.error_message = "No loadable Vulkan libraries found";
        return info;
    }
    
    // PHASE 4: Verify basic Vulkan function loading
    auto vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        runtime_loader.GetSymbol(library_handle, "vkGetInstanceProcAddr"));
    
    if (!vkGetInstanceProcAddr) {
        info.available = false;
        info.error_message = "Failed to load vkGetInstanceProcAddr from " + selected_library;
        return info;
    }
    
    // Try to get vkCreateInstance to verify loader functionality
    auto vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
        vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
    
    if (!vkCreateInstance) {
        info.available = false;
        info.error_message = "Failed to load vkCreateInstance from " + selected_library;
        return info;
    }
    
    // PHASE 5: Success - populate runtime info
    info.available = true;
    info.primary_library_path = selected_library;
    
    // Collect file metadata for the selected library
    struct stat stat_buf;
    if (stat(selected_library.c_str(), &stat_buf) == 0) {
        info.library_file_size = stat_buf.st_size;
        info.library_checksum = std::to_string(stat_buf.st_size) + "_" + std::to_string(stat_buf.st_mtime);
        
        char time_buf[64];
        struct tm* tm_info = localtime(&stat_buf.st_mtime);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
        info.library_last_modified = time_buf;
    }
    
    // Store all candidate paths for reference
    info.library_paths = vulkan_candidates;
    
    // Set Vulkan capabilities
    info.capabilities.jit_compilation = false;  // Vulkan uses precompiled SPIR-V
    info.capabilities.precompiled_kernels = true;
    info.capabilities.memory_management = true;
    info.capabilities.device_enumeration = true;
    info.capabilities.performance_counters = true;
    info.capabilities.supported_targets = {"spirv"};
    info.capabilities.supported_profiles = {"glsl_450", "glsl_460"};
    info.capabilities.supported_stages = {"compute", "vertex", "fragment"};
    
    // Store version information - try to get from loader
    info.version = "Vulkan Loader (Dynamic Detection)";
    
    // Enumerate devices
    info.devices = EnumerateVulkanDevices(info);
    
    KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "Vulkan runtime detection complete: " + selected_library);
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
    std::vector<DeviceInfo> devices;
    
    if (!vulkan_info.available || vulkan_info.primary_library_path.empty()) {
        KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "Vulkan not available, skipping device enumeration");
        return devices;
    }
    
#ifdef KERNTOPIA_VULKAN_SDK_AVAILABLE
    KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "Starting real Vulkan device enumeration with SDK");
    
    // PHASE 1: Load Vulkan library and get function pointers
    RuntimeLoader& runtime_loader = RuntimeLoader::GetInstance();
    auto load_result = runtime_loader.LoadLibrary(vulkan_info.primary_library_path);
    if (!load_result) {
        KERNTOPIA_LOG_ERROR(LogComponent::SYSTEM, "Failed to load Vulkan library for device enumeration: " + vulkan_info.primary_library_path);
        return devices;
    }
    
    void* library_handle = *load_result;
    
    // Get vkGetInstanceProcAddr first
    auto vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        runtime_loader.GetSymbol(library_handle, "vkGetInstanceProcAddr"));
    
    if (!vkGetInstanceProcAddr) {
        KERNTOPIA_LOG_ERROR(LogComponent::SYSTEM, "Failed to get vkGetInstanceProcAddr for device enumeration");
        return devices;
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "Successfully loaded vkGetInstanceProcAddr");
    
    // Get global-level functions (can be called with VK_NULL_HANDLE)
    // Note: Only certain functions can be loaded at global level - vkDestroyInstance is instance-level
    PFN_vkCreateInstance vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
        vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
    
    KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "vkCreateInstance ptr: " + 
                       std::to_string(reinterpret_cast<uintptr_t>(vkCreateInstance)));
    
    if (!vkCreateInstance) {
        KERNTOPIA_LOG_ERROR(LogComponent::SYSTEM, "Failed to get vkCreateInstance at global level");
        return devices;
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "Successfully loaded global Vulkan functions");
    
    // PHASE 2: Create minimal Vulkan instance
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Kerntopia System Interrogator";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "Kerntopia";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;
    
    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledExtensionCount = 0;
    instance_info.ppEnabledExtensionNames = nullptr;
    instance_info.enabledLayerCount = 0;
    instance_info.ppEnabledLayerNames = nullptr;
    
    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&instance_info, nullptr, &instance);
    if (result != VK_SUCCESS) {
        KERNTOPIA_LOG_ERROR(LogComponent::SYSTEM, "Failed to create Vulkan instance for device enumeration: " + std::to_string(result));
        return devices;
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "Vulkan instance created successfully");
    
    // Now load vkDestroyInstance with the valid instance handle
    PFN_vkDestroyInstance vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
        vkGetInstanceProcAddr(instance, "vkDestroyInstance"));
    
    if (!vkDestroyInstance) {
        KERNTOPIA_LOG_ERROR(LogComponent::SYSTEM, "Failed to get vkDestroyInstance with instance handle");
        return devices;
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "vkDestroyInstance loaded: " + 
                       std::to_string(reinterpret_cast<uintptr_t>(vkDestroyInstance)));
    
    // PHASE 3: Get instance-level function pointers (require valid VkInstance handle)
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices"));
    PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties"));
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties"));
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
    
    KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "Instance-level function pointers loaded:");
    KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "  vkEnumeratePhysicalDevices: " + 
                       std::to_string(reinterpret_cast<uintptr_t>(vkEnumeratePhysicalDevices)));
    KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "  vkGetPhysicalDeviceProperties: " + 
                       std::to_string(reinterpret_cast<uintptr_t>(vkGetPhysicalDeviceProperties)));
    
    if (!vkEnumeratePhysicalDevices || !vkGetPhysicalDeviceProperties || 
        !vkGetPhysicalDeviceMemoryProperties || !vkGetPhysicalDeviceQueueFamilyProperties) {
        KERNTOPIA_LOG_ERROR(LogComponent::SYSTEM, "Failed to get required Vulkan device functions");
        vkDestroyInstance(instance, nullptr);
        return devices;
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "All instance-level functions loaded successfully");
    
    // PHASE 4: Enumerate physical devices
    uint32_t device_count = 0;
    result = vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    if (result != VK_SUCCESS || device_count == 0) {
        KERNTOPIA_LOG_WARNING(LogComponent::SYSTEM, "No Vulkan physical devices found");
        vkDestroyInstance(instance, nullptr);
        return devices;
    }
    
    std::vector<VkPhysicalDevice> physical_devices(device_count);
    result = vkEnumeratePhysicalDevices(instance, &device_count, physical_devices.data());
    if (result != VK_SUCCESS) {
        KERNTOPIA_LOG_ERROR(LogComponent::SYSTEM, "Failed to enumerate Vulkan physical devices");
        vkDestroyInstance(instance, nullptr);
        return devices;
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "Found " + std::to_string(device_count) + " Vulkan physical devices");
    
    // PHASE 5: Query each device and create DeviceInfo
    for (uint32_t i = 0; i < device_count; ++i) {
        VkPhysicalDevice physical_device = physical_devices[i];
        
        // Get device properties
        VkPhysicalDeviceProperties device_props = {};
        vkGetPhysicalDeviceProperties(physical_device, &device_props);
        
        // Get memory properties
        VkPhysicalDeviceMemoryProperties memory_props = {};
        vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_props);
        
        // Get queue family properties
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());
        
        // Create DeviceInfo
        DeviceInfo device_info;
        device_info.name = device_props.deviceName;
        device_info.backend_type = Backend::VULKAN;
        
        // Calculate total memory from all device-local heaps
        uint64_t total_memory = 0;
        for (uint32_t j = 0; j < memory_props.memoryHeapCount; ++j) {
            if (memory_props.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                total_memory += memory_props.memoryHeaps[j].size;
            }
        }
        device_info.total_memory_bytes = total_memory;
        
        // Set device-specific information
        device_info.api_version = std::to_string(VK_VERSION_MAJOR(device_props.apiVersion)) + "." +
                                 std::to_string(VK_VERSION_MINOR(device_props.apiVersion)) + "." +
                                 std::to_string(VK_VERSION_PATCH(device_props.apiVersion));
        
        // Store additional device info in the name for now (could enhance DeviceInfo structure later)
        device_info.name = device_props.deviceName;
        if (device_props.vendorID != 0) {
            device_info.name += " (VID: 0x" + std::to_string(device_props.vendorID) + 
                               ", DID: 0x" + std::to_string(device_props.deviceID) + ")";
        }
        
        // Find compute queue family
        bool has_compute = false;
        for (uint32_t j = 0; j < queue_family_count; ++j) {
            if (queue_families[j].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                has_compute = true;
                break;
            }
        }
        
        if (has_compute) {
            device_info.compute_capability = "Vulkan Compute";
            devices.push_back(device_info);
            
            KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, 
                "Device " + std::to_string(i) + ": " + device_info.name + 
                " (" + std::to_string(total_memory / (1024*1024)) + " MB)");
        } else {
            KERNTOPIA_LOG_WARNING(LogComponent::SYSTEM, 
                "Device " + std::to_string(i) + " (" + device_info.name + ") has no compute queues, skipping");
        }
    }
    
    // PHASE 6: Cleanup
    vkDestroyInstance(instance, nullptr);
    
    KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, "Vulkan device enumeration complete: " + std::to_string(devices.size()) + " compute-capable devices found");
    
#else
    // Build was not configured with Vulkan SDK - provide clear feedback
    KERNTOPIA_LOG_WARNING(LogComponent::SYSTEM, 
        "Vulkan device enumeration not available - build was not configured with Vulkan SDK headers");
    KERNTOPIA_LOG_INFO(LogComponent::SYSTEM, 
        "To enable detailed Vulkan device detection, rebuild with KERNTOPIA_VULKAN_SDK_AVAILABLE=ON");
    
    // Return minimal device info indicating SDK limitation
    if (vulkan_info.available) {
        DeviceInfo device;
        device.name = "Vulkan Device (Detailed detection requires SDK)";
        device.backend_type = Backend::VULKAN;
        device.total_memory_bytes = 0;
        device.compute_capability = "Vulkan Compute (Basic detection only)";
        devices.push_back(device);
        
        KERNTOPIA_LOG_DEBUG(LogComponent::SYSTEM, 
            "Added placeholder device info - rebuild with SDK for detailed information");
    }
#endif
    
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