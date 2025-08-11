#include "backend_factory.hpp"
#include "../common/logger.hpp"
#include "cuda_runner.hpp"
#include "vulkan_runner.hpp"

#include <algorithm>

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
    
    // Initialize runtime loader
    runtime_loader_ = std::make_unique<RuntimeLoader>();
    
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
    runtime_loader_.reset();
    
    initialized_ = false;
    LOG_BACKEND_INFO("BackendFactory shut down");
}

Result<void> BackendFactory::DetectBackends() {
    // Detect each backend type
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
    auto scan_result = runtime_loader_->ScanForLibraries(cuda_patterns);
    
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
    auto scan_result = runtime_loader_->ScanForLibraries(vulkan_patterns);
    
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