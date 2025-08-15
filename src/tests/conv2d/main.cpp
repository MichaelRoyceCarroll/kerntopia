#include "conv2d_core.hpp"
#include "core/backend/backend_factory.hpp"
#include "core/common/test_params.hpp"
#include "core/common/logger.hpp"
#include <iostream>

int main() {
    std::cout << "Kerntopia Conv2D Standalone Test" << std::endl;
    std::cout << "=================================" << std::endl;
    
    // Initialize logger
    kerntopia::Logger::Config log_config;
    log_config.min_level = kerntopia::LogLevel::INFO;
    log_config.log_to_console = true;
    kerntopia::Logger::Initialize(log_config);
    
    // Set up test configuration - same as GTest harness
    kerntopia::TestConfiguration config;
    config.target_backend = kerntopia::Backend::CUDA;
    config.slang_profile = kerntopia::SlangProfile::CUDA_SM_7_0;
    config.slang_target = kerntopia::SlangTarget::PTX;
    config.compilation_mode = kerntopia::CompilationMode::PRECOMPILED;
    config.mode = kerntopia::TestMode::FUNCTIONAL;
    config.custom_width = 512;
    config.custom_height = 512;
    config.size = kerntopia::TestSize::CUSTOM;
    
    const std::string input_path = "/home/mcarr/kerntopia/assets/images/StockSnap_2Q79J32WX2_512x512.png";
    const std::string output_path = "conv2d_output.png";
    
    std::cout << "Configuration: " << config.GetBackendName() 
              << " (" << config.GetSlangProfileName() << " -> " 
              << config.GetSlangTargetName() << ")" << std::endl;
    std::cout << "Kernel file: " << config.GetCompiledKernelFilename("conv2d") << std::endl;
    std::cout << "Input image: " << input_path << std::endl;
    std::cout << "Output image: " << output_path << std::endl;
    std::cout << std::endl;
    
    // Initialize backend factory - Conv2dCore will create runner internally
    kerntopia::BackendFactory::Initialize();
    
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