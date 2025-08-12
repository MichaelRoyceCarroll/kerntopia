#include "system_info_service.hpp"
#include "../common/logger.hpp"

namespace kerntopia {

void SystemInfoService::ShowSystemInfo(bool verbose, std::ostream& stream) {
    stream << "System Information\n";
    stream << "==================\n\n";
    
    // Get system information from SystemInterrogator
    auto system_info_result = SystemInterrogator::GetSystemInfo();
    if (!system_info_result) {
        stream << "Error: Failed to get system information\n";
        stream << "Error details: " << system_info_result.GetError().message << "\n";
        return;
    }
    
    const auto& system_info = *system_info_result;
    
    // Count available runtimes
    std::vector<std::string> available_runtimes = system_info.GetAvailableRuntimes();
    stream << "Available Backends: " << available_runtimes.size() << "\n";
    
    // Initialize BackendFactory for device enumeration (legacy compatibility)
    auto init_result = BackendFactory::Initialize();
    if (!init_result) {
        stream << "Warning: Failed to initialize backend system for device enumeration\n";
    }
    
    // Display CUDA runtime
    if (system_info.cuda_runtime.available) {
        DisplayRuntimeInfo(system_info.cuda_runtime, "CUDA", verbose, stream);
        if (verbose && init_result) {
            DisplayDevices(Backend::CUDA, verbose, stream);
        }
    }
    
    // Display Vulkan runtime
    if (system_info.vulkan_runtime.available) {
        DisplayRuntimeInfo(system_info.vulkan_runtime, "Vulkan", verbose, stream);
        if (verbose && init_result) {
            DisplayDevices(Backend::VULKAN, verbose, stream);
        }
    }
    
    // Display CPU runtime (always available)
    stream << "  • CPU (Software) (v1.0.0)\n";
    if (verbose) {
        stream << "    Library: built-in\n";
        stream << "    File Size: 0 bytes\n\n";
    }
    
    // Show unavailable backends
    if (verbose) {
        DisplayUnavailableBackends(system_info, verbose, stream);
    }
    
    // Show SLANG compiler information
    DisplaySlangInfo(system_info.slang_runtime, verbose, stream);
    
    stream << "\nFor detailed backend information, use: kerntopia info --verbose\n";
    
    if (init_result) {
        BackendFactory::Shutdown();
    }
}

void SystemInfoService::ShowBackendsOnly(bool verbose, std::ostream& stream) {
    auto system_info_result = SystemInterrogator::GetSystemInfo();
    if (!system_info_result) {
        stream << "Error: Failed to get system information\n";
        return;
    }
    
    const auto& system_info = *system_info_result;
    std::vector<std::string> available_runtimes = system_info.GetAvailableRuntimes();
    
    stream << "Available Backends: " << available_runtimes.size() << "\n";
    
    if (system_info.cuda_runtime.available) {
        DisplayRuntimeInfo(system_info.cuda_runtime, "CUDA", verbose, stream);
    }
    
    if (system_info.vulkan_runtime.available) {
        DisplayRuntimeInfo(system_info.vulkan_runtime, "Vulkan", verbose, stream);
    }
    
    stream << "  • CPU (Software) (v1.0.0)\n";
}

void SystemInfoService::ShowSlangOnly(bool verbose, std::ostream& stream) {
    auto system_info_result = SystemInterrogator::GetSystemInfo();
    if (!system_info_result) {
        stream << "Error: Failed to get system information\n";
        return;
    }
    
    const auto& system_info = *system_info_result;
    DisplaySlangInfo(system_info.slang_runtime, verbose, stream);
}

Result<SystemInfo> SystemInfoService::GetSystemInformation() {
    return SystemInterrogator::GetSystemInfo();
}

void SystemInfoService::DisplayRuntimeInfo(const RuntimeInfo& runtime, const std::string& runtime_name, 
                                          bool verbose, std::ostream& stream) {
    stream << "  • " << runtime.name;
    if (!runtime.version.empty()) {
        stream << " (v" << runtime.version << ")";
    }
    stream << "\n";
    
    if (verbose) {
        stream << "    Library: " << runtime.primary_library_path << "\n";
        if (!runtime.library_checksum.empty()) {
            stream << "    Checksum: " << runtime.library_checksum.substr(0, 16) << "...\n";
        }
        stream << "    File Size: " << runtime.library_file_size << " bytes\n";
        if (!runtime.library_last_modified.empty()) {
            stream << "    Modified: " << runtime.library_last_modified << "\n";
        }
        stream << "\n";
    }
}

void SystemInfoService::DisplaySlangInfo(const RuntimeInfo& slang, bool verbose, std::ostream& stream) {
    stream << "\nSLANG Compiler\n";
    stream << "==============\n";
    
    if (slang.available) {
        stream << "  Status: Available\n";
        stream << "  Version: " << slang.version << "\n";
        
        // Report capabilities for JIT vs Precompiled modes
        if (slang.capabilities.jit_compilation && slang.capabilities.precompiled_kernels) {
            stream << "  Mode: JIT + Precompiled (Both libslang.so and slangc available)\n";
        } else if (slang.capabilities.jit_compilation) {
            stream << "  Mode: JIT Only (libslang.so available, slangc missing)\n";
        } else if (slang.capabilities.precompiled_kernels) {
            stream << "  Mode: Precompiled Only (slangc available, libslang.so missing)\n";
        } else {
            stream << "  Mode: Limited (Neither JIT nor precompiled fully available)\n";
        }
        
        if (verbose) {
            if (!slang.primary_executable_path.empty()) {
                stream << "  Executable: " << slang.primary_executable_path << "\n";
                stream << "  Executable Size: " << slang.executable_file_size << " bytes\n";
                if (!slang.executable_last_modified.empty()) {
                    stream << "  Executable Modified: " << slang.executable_last_modified << "\n";
                }
            }
            
            if (!slang.primary_library_path.empty()) {
                stream << "  Runtime Library: " << slang.primary_library_path << "\n";
                stream << "  Library Size: " << slang.library_file_size << " bytes\n";
            }
            
            if (!slang.capabilities.supported_targets.empty()) {
                stream << "  Supported Targets: ";
                for (size_t i = 0; i < slang.capabilities.supported_targets.size(); ++i) {
                    if (i > 0) stream << ", ";
                    stream << slang.capabilities.supported_targets[i];
                }
                stream << "\n";
            }
            
            if (!slang.capabilities.supported_profiles.empty()) {
                stream << "  Supported Profiles: ";
                for (size_t i = 0; i < slang.capabilities.supported_profiles.size(); ++i) {
                    if (i > 0) stream << ", ";
                    stream << slang.capabilities.supported_profiles[i];
                }
                stream << "\n";
            }
        }
    } else {
        stream << "  Status: Not Available\n";
        stream << "  Error: " << slang.error_message << "\n";
    }
}

void SystemInfoService::DisplayDevices(Backend backend, bool verbose, std::ostream& stream) {
    auto devices_result = BackendFactory::GetDevices(backend);
    if (devices_result) {
        const auto& devices = *devices_result;
        stream << "    Devices: " << devices.size() << "\n";
        for (size_t i = 0; i < devices.size(); ++i) {
            const auto& device = devices[i];
            stream << "      [" << i << "] " << device.name;
            if (device.total_memory_bytes > 0) {
                double memory_gb = static_cast<double>(device.total_memory_bytes) / (1024.0 * 1024.0 * 1024.0);
                stream << " (" << std::fixed << std::setprecision(1) << memory_gb << " GB)";
            }
            stream << "\n";
        }
    }
}

void SystemInfoService::DisplayUnavailableBackends(const SystemInfo& system_info, bool verbose, std::ostream& stream) {
    stream << "\nUnavailable Backends:\n";
    if (!system_info.cuda_runtime.available) {
        stream << "  • CUDA - " << system_info.cuda_runtime.error_message << "\n";
    }
    if (!system_info.vulkan_runtime.available) {
        stream << "  • Vulkan - " << system_info.vulkan_runtime.error_message << "\n";
    }
}

} // namespace kerntopia