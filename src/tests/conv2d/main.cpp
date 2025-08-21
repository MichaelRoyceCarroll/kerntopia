#include "conv2d_core.hpp"
#include "core/backend/backend_factory.hpp"
#include "core/common/test_params.hpp"
#include "core/common/logger.hpp"
#include "core/common/path_utils.hpp"
#include <iostream>
#include <string>

void PrintUsage() {
    std::cout << "Usage: kerntopia-conv2d [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --backend <name>    Target backend (cuda, vulkan, cpu)\n";
    std::cout << "  --device <id>       Target device ID (use after --backend)\n";
    std::cout << "  --help              Show this help message\n";
    std::cout << "Examples:\n";
    std::cout << "  kerntopia-conv2d --backend cuda --device 0\n";
    std::cout << "  kerntopia-conv2d --backend vulkan --device 1\n";
}

int main(int argc, char* argv[]) {
    std::cout << "Kerntopia Conv2D Standalone Test" << std::endl;
    std::cout << "=================================" << std::endl;
    
    // Parse command line arguments
    kerntopia::Backend backend = kerntopia::Backend::CUDA;  // Default
    int device_id = 0;  // Default
    bool backend_specified = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            PrintUsage();
            return 0;
        }
        else if (arg == "--backend") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --backend requires argument\n";
                return 1;
            }
            std::string backend_str = argv[++i];
            if (backend_str == "cuda") {
                backend = kerntopia::Backend::CUDA;
            } else if (backend_str == "vulkan") {
                backend = kerntopia::Backend::VULKAN;
            } else if (backend_str == "cpu") {
                backend = kerntopia::Backend::CPU;
            } else {
                std::cerr << "Error: Unknown backend '" << backend_str << "'. Valid options: cuda, vulkan, cpu\n";
                return 1;
            }
            backend_specified = true;
        }
        else if (arg == "--device") {
            if (!backend_specified) {
                std::cerr << "Error: --device can only be used after --backend is specified\n";
                return 1;
            }
            if (i + 1 >= argc) {
                std::cerr << "Error: --device requires argument\n";
                return 1;
            }
            try {
                device_id = std::stoi(argv[++i]);
                if (device_id < 0) {
                    std::cerr << "Error: Device ID must be non-negative\n";
                    return 1;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid device ID '" << argv[i] << "'. Must be a non-negative integer\n";
                return 1;
            }
        }
        else {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            PrintUsage();
            return 1;
        }
    }
    
    // Initialize logger
    kerntopia::Logger::Config log_config;
    log_config.min_level = kerntopia::LogLevel::INFO;
    log_config.log_to_console = true;
    kerntopia::Logger::Initialize(log_config);
    
    // Initialize backend factory to detect available backends
    kerntopia::BackendFactory::Initialize();
    
    // If no backend was specified, pick the first available backend
    if (!backend_specified) {
        auto available_backends = kerntopia::BackendFactory::GetAvailableBackends();
        if (available_backends.empty()) {
            std::cerr << "Error: No backends available on this system\n";
            kerntopia::BackendFactory::Shutdown();
            kerntopia::Logger::Shutdown();
            return 1;
        }
        backend = available_backends[0]; // Pick first available backend
        std::cout << "No backend specified, using first available: ";
        kerntopia::TestConfiguration temp_config;
        temp_config.target_backend = backend;
        std::cout << temp_config.GetBackendName() << std::endl;
    }
    
    // Set up test configuration with parsed values
    kerntopia::TestConfiguration config;
    config.target_backend = backend;
    config.device_id = device_id;
    config.compilation_mode = kerntopia::CompilationMode::PRECOMPILED;
    config.mode = kerntopia::TestMode::FUNCTIONAL;
    config.custom_width = 512;
    config.custom_height = 512;
    config.size = kerntopia::TestSize::CUSTOM;
    
    // Set profile and target based on backend
    switch (backend) {
        case kerntopia::Backend::CUDA:
            config.slang_profile = kerntopia::SlangProfile::CUDA_SM_7_0;
            config.slang_target = kerntopia::SlangTarget::PTX;
            break;
        case kerntopia::Backend::VULKAN:
        case kerntopia::Backend::CPU:
            config.slang_profile = kerntopia::SlangProfile::GLSL_450;
            config.slang_target = kerntopia::SlangTarget::SPIRV;
            break;
        default:
            config.slang_profile = kerntopia::SlangProfile::CUDA_SM_7_0;
            config.slang_target = kerntopia::SlangTarget::PTX;
            break;
    }
    
    const std::string input_path = kerntopia::PathUtils::GetAssetsDirectory() + "images/StockSnap_2Q79J32WX2_512x512.png";
    const std::string output_path = config.GetOutputPrefix() + "_conv2d_output.png";
    
    std::cout << "Configuration: " << config.GetBackendName() 
              << " (" << config.GetSlangProfileName() << " -> " 
              << config.GetSlangTargetName() << ")" << std::endl;
    std::cout << "Device ID: " << config.device_id << std::endl;
    std::cout << "Kernel file: " << config.GetCompiledKernelFilename("conv2d") << std::endl;
    std::cout << "Input image: " << input_path << std::endl;
    std::cout << "Output image: " << output_path << std::endl;
    std::cout << std::endl;
    
    // Backend factory already initialized above
    
    // Use same Conv2DCore code path as GTest harness - it handles backend creation internally
    kerntopia::conv2d::Conv2dCore conv2d(config);
    
    // Execute pipeline - same interface as GTest harness
    auto setup_result = conv2d.Setup(input_path);
    if (!setup_result) {
        std::cerr << "Setup failed: " << setup_result.GetError().message << std::endl;
        kerntopia::BackendFactory::Shutdown();
        kerntopia::Logger::Shutdown();
        return 1;
    }
    
    auto execute_result = conv2d.Execute();
    if (!execute_result) {
        std::cerr << "Execution failed: " << execute_result.GetError().message << std::endl;
        kerntopia::BackendFactory::Shutdown();
        kerntopia::Logger::Shutdown();
        return 1;
    }
    
    auto write_result = conv2d.WriteOut(output_path);
    if (!write_result) {
        std::cerr << "Write output failed: " << write_result.GetError().message << std::endl;
        kerntopia::BackendFactory::Shutdown();
        kerntopia::Logger::Shutdown();
        return 1;
    }
    
    std::cout << std::endl;
    std::cout << "Success! Check " << output_path << " for the blurred result." << std::endl;
    std::cout << "Pipeline: Setup -> Execute -> WriteOut -> TearDown" << std::endl;
    std::cout << "Same code path as GTest harness - no duplication!" << std::endl;
    
    kerntopia::BackendFactory::Shutdown();
    kerntopia::Logger::Shutdown();
    return 0;
}