#include "backend_factory.hpp"
#include "../common/logger.hpp"
#include "cuda_runner.hpp"
#include "vulkan_runner.hpp"

#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <ctime>

namespace kerntopia {

// Static member definitions
std::unique_ptr<BackendFactory> BackendFactory::instance_ = nullptr;
std::mutex BackendFactory::instance_mutex_;

Result<void> BackendFactory::Initialize() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (instance_) {
        return KERNTOPIA_VOID_SUCCESS(); // Already initialized
    }
    
    instance_ = std::unique_ptr<BackendFactory>(new BackendFactory());
    return instance_->InitializeImpl();
}

void BackendFactory::Shutdown() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (instance_) {
        instance_->ShutdownImpl();
        instance_.reset();
    }
}

BackendFactory& BackendFactory::GetInstance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) {
        // Auto-initialize if not done explicitly
        instance_ = std::unique_ptr<BackendFactory>(new BackendFactory());
        auto result = instance_->InitializeImpl();
        if (!result) {
            LOG_BACKEND_ERROR("Failed to auto-initialize BackendFactory");
        }
    }
    return *instance_;
}

std::vector<Backend> BackendFactory::GetAvailableBackends() {
    auto& factory = GetInstance();
    std::lock_guard<std::mutex> lock(factory.mutex_);
    
    std::vector<Backend> available;
    for (const auto& [backend, info] : factory.backend_info_) {
        if (info.available) {
            available.push_back(backend);
        }
    }
    
    return available;
}

std::map<Backend, BackendInfo> BackendFactory::GetBackendInfo() {
    auto& factory = GetInstance();
    std::lock_guard<std::mutex> lock(factory.mutex_);
    return factory.backend_info_;
}

Result<BackendInfo> BackendFactory::GetBackendInfo(Backend backend) {
    auto& factory = GetInstance();
    std::lock_guard<std::mutex> lock(factory.mutex_);
    
    auto it = factory.backend_info_.find(backend);
    if (it == factory.backend_info_.end()) {
        return KERNTOPIA_RESULT_ERROR(BackendInfo, ErrorCategory::BACKEND,
                                     ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Backend not found: " + backend_utils::ToString(backend));
    }
    
    return KERNTOPIA_SUCCESS(it->second);
}

bool BackendFactory::IsBackendAvailable(Backend backend) {
    auto info_result = GetBackendInfo(backend);
    return info_result && info_result->available;
}

Result<std::vector<DeviceInfo>> BackendFactory::GetDevices(Backend backend) {
    auto& factory = GetInstance();
    std::lock_guard<std::mutex> lock(factory.mutex_);
    
    auto factory_result = factory.GetFactory(backend);
    if (!factory_result) {
        return KERNTOPIA_RESULT_ERROR(std::vector<DeviceInfo>, ErrorCategory::BACKEND,
                                     ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Backend factory not available");
    }
    
    auto devices = (*factory_result)->EnumerateDevices();
    return KERNTOPIA_SUCCESS(devices);
}

Result<std::unique_ptr<IKernelRunner>> BackendFactory::CreateRunner(Backend backend, int device_id) {
    auto& factory = GetInstance();
    std::lock_guard<std::mutex> lock(factory.mutex_);
    
    auto factory_result = factory.GetFactory(backend);
    if (!factory_result) {
        return KERNTOPIA_RESULT_ERROR(std::unique_ptr<IKernelRunner>, ErrorCategory::BACKEND,
                                     ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Backend factory not available");
    }
    
    return (*factory_result)->CreateRunner(device_id);
}

// Removed duplicate method - implementation is in private section

// Private implementation methods
Result<void> BackendFactory::InitializeImpl() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return KERNTOPIA_VOID_SUCCESS();
    }
    
    // Initialize runtime loader (now using singleton)
    RuntimeLoader& runtime_loader = RuntimeLoader::GetInstance();
    
    // Detect all backends
    auto detect_result = DetectBackends();
    if (!detect_result) {
        LOG_BACKEND_ERROR("Backend detection failed");
        return detect_result;
    }
    
    initialized_ = true;
    LOG_BACKEND_INFO("BackendFactory initialized successfully");
    
    return KERNTOPIA_VOID_SUCCESS();
}

void BackendFactory::ShutdownImpl() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    factories_.clear();
    backend_info_.clear();
    // Note: RuntimeLoader is now a singleton, no manual cleanup needed
    
    initialized_ = false;
    LOG_BACKEND_INFO("BackendFactory shut down");
}

Result<void> BackendFactory::DetectBackends() {
    // **NEW**: Use SystemInterrogator for unified detection
    auto system_info_result = SystemInterrogator::GetSystemInfo();
    if (!system_info_result) {
        LOG_BACKEND_ERROR("Failed to get system information from SystemInterrogator");
        // Fallback to legacy detection
        return DetectBackendsLegacy();
    }
    
    const auto& system_info = *system_info_result;
    
    // Convert RuntimeInfo to BackendInfo for backward compatibility
    if (system_info.cuda_runtime.available) {
        backend_info_[Backend::CUDA] = ConvertRuntimeToBackend(system_info.cuda_runtime, Backend::CUDA);
        LOG_BACKEND_INFO("Detected backend: CUDA");
    } else {
        BackendInfo info;
        info.type = Backend::CUDA;
        info.name = "CUDA";
        info.available = false;
        info.error_message = system_info.cuda_runtime.error_message;
        backend_info_[Backend::CUDA] = info;
        KERNTOPIA_LOG_WARNING(LogComponent::BACKEND, "Backend unavailable: CUDA - " + info.error_message);
    }
    
    if (system_info.vulkan_runtime.available) {
        backend_info_[Backend::VULKAN] = ConvertRuntimeToBackend(system_info.vulkan_runtime, Backend::VULKAN);
        LOG_BACKEND_INFO("Detected backend: Vulkan");
    } else {
        BackendInfo info;
        info.type = Backend::VULKAN;
        info.name = "Vulkan";
        info.available = false;
        info.error_message = system_info.vulkan_runtime.error_message;
        backend_info_[Backend::VULKAN] = info;
        KERNTOPIA_LOG_WARNING(LogComponent::BACKEND, "Backend unavailable: Vulkan - " + info.error_message);
    }
    
    // CPU backend (always available)
    BackendInfo cpu_info;
    cpu_info.type = Backend::CPU;
    cpu_info.name = "CPU (Software)";
    cpu_info.available = true;
    cpu_info.version = "1.0.0";
    cpu_info.library_path = "built-in";
    backend_info_[Backend::CPU] = cpu_info;
    LOG_BACKEND_INFO("Detected backend: CPU");
    
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> BackendFactory::DetectBackendsLegacy() {
    // Legacy detection fallback
    std::vector<Backend> backends_to_detect = {
        Backend::CUDA,
        Backend::VULKAN,
        Backend::CPU
    };
    
    for (Backend backend : backends_to_detect) {
        auto info_result = DetectBackend(backend);
        if (info_result) {
            backend_info_[backend] = *info_result;
            LOG_BACKEND_INFO("Detected backend: " + backend_utils::ToString(backend));
        } else {
            // Create unavailable entry
            BackendInfo info;
            info.type = backend;
            info.name = backend_utils::ToString(backend);
            info.available = false;
            info.error_message = info_result.GetError().message;
            backend_info_[backend] = info;
            KERNTOPIA_LOG_WARNING(LogComponent::BACKEND, "Backend unavailable: " + backend_utils::ToString(backend) + " - " + info.error_message);
        }
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

Result<BackendInfo> BackendFactory::DetectBackend(Backend backend) {
    switch (backend) {
        case Backend::CUDA:
            return DetectCudaBackend();
        case Backend::VULKAN:
            return DetectVulkanBackend();
        case Backend::CPU:
            return DetectCpuBackend();
        default:
            return KERNTOPIA_RESULT_ERROR(BackendInfo, ErrorCategory::BACKEND,
                                         ErrorCode::BACKEND_NOT_AVAILABLE,
                                         "Unknown backend type");
    }
}

Result<BackendInfo> BackendFactory::DetectCudaBackend() {
    BackendInfo info;
    info.type = Backend::CUDA;
    info.name = "CUDA";
    
    // Search for CUDA runtime libraries
    std::vector<std::string> cuda_patterns = {"cudart", "nvcuda"};
    RuntimeLoader& runtime_loader = RuntimeLoader::GetInstance();
    auto scan_result = runtime_loader.ScanForLibraries(cuda_patterns);
    
    if (!scan_result || scan_result->empty()) {
        info.available = false;
        info.error_message = "CUDA runtime libraries not found";
        return KERNTOPIA_SUCCESS(info); // Not an error, just unavailable
    }
    
    // Check for primary CUDA runtime
    auto& libraries = *scan_result;
    auto cudart_it = std::find_if(libraries.begin(), libraries.end(),
        [](const auto& pair) { return pair.first.find("cudart") != std::string::npos; });
    
    if (cudart_it != libraries.end()) {
        info.available = true;
        info.library_path = cudart_it->second.full_path;
        info.version = cudart_it->second.version;
        info.checksum = cudart_it->second.checksum;
        info.file_size = cudart_it->second.file_size;
        info.last_modified = cudart_it->second.last_modified;
        
        // Collect all paths
        for (const auto& [name, lib_info] : libraries) {
            info.library_paths.push_back(lib_info.full_path);
        }
    } else {
        info.available = false;
        info.error_message = "CUDA runtime not found in detected libraries";
    }
    
    return KERNTOPIA_SUCCESS(info);
}

Result<BackendInfo> BackendFactory::DetectVulkanBackend() {
    BackendInfo info;
    info.type = Backend::VULKAN;
    info.name = "Vulkan";
    
    // Search for Vulkan libraries
    std::vector<std::string> vulkan_patterns = {"vulkan"};
    RuntimeLoader& runtime_loader = RuntimeLoader::GetInstance();
    auto scan_result = runtime_loader.ScanForLibraries(vulkan_patterns);
    
    if (!scan_result || scan_result->empty()) {
        info.available = false;
        info.error_message = "Vulkan libraries not found";
        return KERNTOPIA_SUCCESS(info);
    }
    
    // Take first available Vulkan library
    auto& libraries = *scan_result;
    if (!libraries.empty()) {
        auto& first_lib = libraries.begin()->second;
        info.available = true;
        info.library_path = first_lib.full_path;
        info.version = first_lib.version;
        info.checksum = first_lib.checksum;
        info.file_size = first_lib.file_size;
        info.last_modified = first_lib.last_modified;
        
        for (const auto& [name, lib_info] : libraries) {
            info.library_paths.push_back(lib_info.full_path);
        }
    }
    
    return KERNTOPIA_SUCCESS(info);
}

Result<BackendInfo> BackendFactory::DetectCpuBackend() {
    BackendInfo info;
    info.type = Backend::CPU;
    info.name = "CPU (Software)";
    info.available = true; // CPU backend is always available
    info.version = "1.0.0";
    info.library_path = "built-in";
    
    return KERNTOPIA_SUCCESS(info);
}

SlangCompilerInfo BackendFactory::GetSlangCompilerInfo() {
    auto& instance = GetInstance();
    return instance.DetectSlangCompiler();
}

SlangCompilerInfo BackendFactory::DetectSlangCompiler() {
    SlangCompilerInfo info;
    
    // Check for slangc executable in PATH and common locations
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
        
        // Check if file exists and is executable
        if (access(candidate.c_str(), F_OK) == 0) {
            if (access(candidate.c_str(), X_OK) == 0) {
                slangc_path = candidate;
                break;
            }
        }
    }
    
    if (slangc_path.empty()) {
        info.available = false;
        info.error_message = "slangc executable not found in PATH or build directory";
        return info;
    }
    
    info.slangc_path = slangc_path;
    
    // Get file information for slangc
    struct stat stat_buf;
    if (stat(slangc_path.c_str(), &stat_buf) == 0) {
        info.slangc_file_size = stat_buf.st_size;
        
        // Format last modified time
        char time_str[100];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", 
                localtime(&stat_buf.st_mtime));
        info.slangc_last_modified = time_str;
        
        // Calculate checksum (simplified - could use proper SHA256)
        info.slangc_checksum = std::to_string(stat_buf.st_size) + "_" + 
                              std::to_string(stat_buf.st_mtime);
    }
    
    // Try to get version from slangc -h (since --version is not supported)
    FILE* version_pipe = popen((slangc_path + " -h 2>&1").c_str(), "r");
    if (version_pipe) {
        char buffer[256];
        std::string version_output;
        
        while (fgets(buffer, sizeof(buffer), version_pipe)) {
            version_output += buffer;
        }
        pclose(version_pipe);
        
        // Look for version in help output
        if (version_output.find("slang") != std::string::npos) {
            info.version = "2025.14.3"; // Use the known version from FetchContent
        } else {
            info.version = "unknown";
        }
    } else {
        info.version = "unknown";
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
        
        // Parse common targets from help output
        if (targets_output.find("spirv") != std::string::npos) {
            info.supported_targets.push_back("spirv");
        }
        if (targets_output.find("ptx") != std::string::npos) {
            info.supported_targets.push_back("ptx");
        }
        if (targets_output.find("dxil") != std::string::npos) {
            info.supported_targets.push_back("dxil");
        }
        if (targets_output.find("glsl") != std::string::npos) {
            info.supported_targets.push_back("glsl");
        }
    }
    
    // Get supported profiles from slangc -h profile
    FILE* profiles_pipe = popen((slangc_path + " -h profile 2>&1").c_str(), "r");
    if (profiles_pipe) {
        char buffer[256];
        std::string profiles_output;
        
        while (fgets(buffer, sizeof(buffer), profiles_pipe)) {
            profiles_output += buffer;
        }
        pclose(profiles_pipe);
        
        // Parse common profiles from help output
        if (profiles_output.find("glsl_450") != std::string::npos) {
            info.supported_profiles.push_back("glsl_450");
        }
        if (profiles_output.find("sm_") != std::string::npos) {
            info.supported_profiles.push_back("sm_6_0");
            info.supported_profiles.push_back("sm_6_5");
        }
    }
    
    // Search for SLANG runtime library
    std::vector<std::string> slang_lib_patterns = {"slang", "libslang"};
    RuntimeLoader& runtime_loader = RuntimeLoader::GetInstance();
    auto scan_result = runtime_loader.ScanForLibraries(slang_lib_patterns);
    
    if (scan_result && !scan_result->empty()) {
        auto& libraries = *scan_result;
        if (!libraries.empty()) {
            auto& first_lib = libraries.begin()->second;
            info.library_path = first_lib.full_path;
            info.library_file_size = first_lib.file_size;
            info.library_last_modified = first_lib.last_modified;
            info.library_checksum = first_lib.checksum;
            
            for (const auto& [name, lib_info] : libraries) {
                info.library_paths.push_back(lib_info.full_path);
            }
        }
    }
    
    info.available = true;
    return info;
}

Result<SystemInfo> BackendFactory::GetSystemInterrogation() {
    return SystemInterrogator::GetSystemInfo();
}

BackendInfo BackendFactory::ConvertRuntimeToBackend(const RuntimeInfo& runtime_info, Backend backend_type) {
    BackendInfo backend_info;
    
    backend_info.type = backend_type;
    backend_info.name = runtime_info.name;
    backend_info.available = runtime_info.available;
    backend_info.version = runtime_info.version;
    backend_info.error_message = runtime_info.error_message;
    
    // File system information
    backend_info.library_path = runtime_info.primary_library_path;
    backend_info.library_paths = runtime_info.library_paths;
    backend_info.checksum = runtime_info.library_checksum;
    backend_info.file_size = runtime_info.library_file_size;
    backend_info.last_modified = runtime_info.library_last_modified;
    
    // Default to primary runtime
    backend_info.is_primary = true;
    
    return backend_info;
}

Result<std::shared_ptr<IKernelRunnerFactory>> BackendFactory::GetFactory(Backend backend) {
    auto& instance = GetInstance();
    return instance.GetFactoryImpl(backend);
}

Result<std::shared_ptr<IKernelRunnerFactory>> BackendFactory::GetFactoryImpl(Backend backend) {
    auto it = factories_.find(backend);
    if (it != factories_.end()) {
        return KERNTOPIA_SUCCESS(it->second);
    }
    
    // Create factory on demand
    Result<std::shared_ptr<IKernelRunnerFactory>> factory_result = 
        KERNTOPIA_RESULT_ERROR(std::shared_ptr<IKernelRunnerFactory>, ErrorCategory::BACKEND,
                              ErrorCode::BACKEND_NOT_AVAILABLE, "Backend not implemented");
    
    switch (backend) {
        case Backend::CUDA:
            factory_result = CreateCudaFactory();
            break;
        case Backend::VULKAN:
            factory_result = CreateVulkanFactory();
            break;
        case Backend::CPU:
            factory_result = CreateCpuFactory();
            break;
    }
    
    if (factory_result) {
        factories_[backend] = *factory_result;
        return factory_result;
    }
    
    return factory_result;
}

// Factory creation methods
Result<std::shared_ptr<IKernelRunnerFactory>> BackendFactory::CreateCudaFactory() {
    auto factory = std::make_shared<CudaKernelRunnerFactory>();
    if (!factory->IsAvailable()) {
        return KERNTOPIA_RESULT_ERROR(std::shared_ptr<IKernelRunnerFactory>, ErrorCategory::BACKEND,
                                     ErrorCode::BACKEND_NOT_AVAILABLE, "CUDA backend not available on this system");
    }
    
    LOG_BACKEND_INFO("Created CUDA backend factory");
    return KERNTOPIA_SUCCESS(std::static_pointer_cast<IKernelRunnerFactory>(factory));
}

Result<std::shared_ptr<IKernelRunnerFactory>> BackendFactory::CreateVulkanFactory() {
    auto factory = std::make_shared<VulkanKernelRunnerFactory>();
    if (!factory->IsAvailable()) {
        return KERNTOPIA_RESULT_ERROR(std::shared_ptr<IKernelRunnerFactory>, ErrorCategory::BACKEND,
                                     ErrorCode::BACKEND_NOT_AVAILABLE, "Vulkan backend not available on this system");
    }
    
    LOG_BACKEND_INFO("Created Vulkan backend factory");
    return KERNTOPIA_SUCCESS(std::static_pointer_cast<IKernelRunnerFactory>(factory));
}

Result<std::shared_ptr<IKernelRunnerFactory>> BackendFactory::CreateCpuFactory() {
    return KERNTOPIA_RESULT_ERROR(std::shared_ptr<IKernelRunnerFactory>, ErrorCategory::BACKEND,
                                 ErrorCode::BACKEND_NOT_AVAILABLE, "CPU factory not implemented yet");
}

// Backend utility functions
namespace backend_utils {

std::string ToString(Backend backend) {
    switch (backend) {
        case Backend::CUDA:   return "CUDA";
        case Backend::VULKAN: return "Vulkan";
        case Backend::CPU:    return "CPU";
        case Backend::DX12:   return "DirectX12";
        default:              return "Unknown";
    }
}

Result<Backend> FromString(const std::string& name) {
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    
    if (lower_name == "cuda") return KERNTOPIA_SUCCESS(Backend::CUDA);
    if (lower_name == "vulkan") return KERNTOPIA_SUCCESS(Backend::VULKAN);
    if (lower_name == "cpu") return KERNTOPIA_SUCCESS(Backend::CPU);
    if (lower_name == "dx12" || lower_name == "directx12") return KERNTOPIA_SUCCESS(Backend::DX12);
    
    return KERNTOPIA_RESULT_ERROR(Backend, ErrorCategory::VALIDATION,
                                 ErrorCode::INVALID_ARGUMENT,
                                 "Unknown backend name: " + name);
}

std::vector<Backend> GetAllBackends() {
    return {Backend::CUDA, Backend::VULKAN, Backend::CPU, Backend::DX12};
}

std::vector<Backend> GetDefaultPreferenceOrder() {
    return {Backend::CUDA, Backend::VULKAN, Backend::CPU};
}

bool RequiresSpecificHardware(Backend backend) {
    switch (backend) {
        case Backend::CUDA:   return true;  // Requires NVIDIA GPU
        case Backend::VULKAN: return false; // Can use CPU via llvmpipe
        case Backend::CPU:    return false; // Software only
        case Backend::DX12:   return true;  // Requires GPU
        default:              return true;
    }
}

std::string GetMinimumRequirements(Backend backend) {
    switch (backend) {
        case Backend::CUDA:
            return "NVIDIA GPU with compute capability 6.0+, CUDA Toolkit 11.0+";
        case Backend::VULKAN:
            return "Vulkan 1.1+ drivers (GPU or CPU via llvmpipe)";
        case Backend::CPU:
            return "Any x86_64 CPU";
        case Backend::DX12:
            return "DirectX 12 compatible GPU, Windows 10+";
        default:
            return "Unknown";
    }
}

} // namespace backend_utils

} // namespace kerntopia