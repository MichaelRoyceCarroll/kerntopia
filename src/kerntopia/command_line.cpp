#include "command_line.hpp"
#include <iostream>
#include <sstream>

namespace kerntopia {

bool CommandLineParser::Parse(int argc, char* argv[]) {
    if (argc < 2) {
        help_requested_ = true;
        return false;
    }
    
    std::string command = argv[1];
    
    // Handle info command
    if (command == "info") {
        info_command_ = true;
        // Check for --verbose flag
        for (int i = 2; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--verbose" || arg == "-v") {
                verbose_ = true;
            }
        }
        return true;
    }
    
    // Handle run command
    if (command == "run") {
        if (argc < 3) {
            std::cerr << "Error: 'run' command requires kernel name or 'all'\n";
            return false;
        }
        
        std::string kernel_or_all = argv[2];
        if (kernel_or_all == "all") {
            test_names_ = {"vector_add", "conv2d", "bilateral_filter", "parallel_reduction", "matrix_transpose"};
        } else {
            test_names_ = {kernel_or_all};
        }
        
        // Parse additional options
        return ParseOptions(argc - 3, argv + 3);
    }
    
    // Handle help
    if (command == "help" || command == "--help" || command == "-h") {
        help_requested_ = true;
        return true;
    }
    
    std::cerr << "Error: Unknown command '" << command << "'\n";
    return false;
}

bool CommandLineParser::ParseOptions(int argc, char* argv[]) {
    for (int i = 0; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--backend" || arg == "-b") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --backend requires argument\n";
                return false;
            }
            if (!ParseBackend(argv[++i])) return false;
        }
        else if (arg == "--profile" || arg == "-p") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --profile requires argument\n";
                return false;
            }
            if (!ParseProfile(argv[++i])) return false;
        }
        else if (arg == "--target" || arg == "-t") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --target requires argument\n";
                return false;
            }
            if (!ParseTarget(argv[++i])) return false;
        }
        else if (arg == "--mode" || arg == "-m") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --mode requires argument\n";
                return false;
            }
            if (!ParseMode(argv[++i])) return false;
        }
        else if (arg == "--verbose" || arg == "-v") {
            verbose_ = true;
        }
        else {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        }
    }
    
    // Set defaults for profile/target if not specified
    SetDefaultProfileTarget();
    return true;
}

bool CommandLineParser::ParseBackend(const std::string& backend_str) {
    if (backend_str == "cuda") {
        test_config_.target_backend = Backend::CUDA;
    } else if (backend_str == "vulkan") {
        test_config_.target_backend = Backend::VULKAN;
    } else if (backend_str == "cpu") {
        test_config_.target_backend = Backend::CPU;
    } else if (backend_str == "dx12") {
        test_config_.target_backend = Backend::DX12;
    } else {
        std::cerr << "Error: Unknown backend '" << backend_str << "'. Valid options: cuda, vulkan, cpu, dx12\n";
        return false;
    }
    return true;
}

bool CommandLineParser::ParseProfile(const std::string& profile_str) {
    if (profile_str == "glsl_450") {
        test_config_.slang_profile = SlangProfile::GLSL_450;
    } else if (profile_str == "sm_7_5") {
        test_config_.slang_profile = SlangProfile::SM_7_5;
    } else if (profile_str == "sm_8_9") {
        test_config_.slang_profile = SlangProfile::SM_8_9;
    } else if (profile_str == "hlsl_6_0") {
        test_config_.slang_profile = SlangProfile::HLSL_6_0;
    } else {
        std::cerr << "Error: Unknown profile '" << profile_str << "'. Valid options: glsl_450, sm_7_5, sm_8_9, hlsl_6_0\n";
        return false;
    }
    return true;
}

bool CommandLineParser::ParseTarget(const std::string& target_str) {
    if (target_str == "spirv") {
        test_config_.slang_target = SlangTarget::SPIRV;
    } else if (target_str == "ptx") {
        test_config_.slang_target = SlangTarget::PTX;
    } else if (target_str == "glsl") {
        test_config_.slang_target = SlangTarget::GLSL;
    } else if (target_str == "hlsl") {
        test_config_.slang_target = SlangTarget::HLSL;
    } else {
        std::cerr << "Error: Unknown target '" << target_str << "'. Valid options: spirv, ptx, glsl, hlsl\n";
        return false;
    }
    return true;
}

bool CommandLineParser::ParseMode(const std::string& mode_str) {
    if (mode_str == "functional") {
        test_config_.mode = TestMode::FUNCTIONAL;
    } else if (mode_str == "performance") {
        test_config_.mode = TestMode::PERFORMANCE;
    } else {
        std::cerr << "Error: Unknown mode '" << mode_str << "'. Valid options: functional, performance\n";
        return false;
    }
    return true;
}

void CommandLineParser::SetDefaultProfileTarget() {
    // Set defaults based on backend if not explicitly specified
    if (test_config_.slang_profile == SlangProfile::DEFAULT) {
        switch (test_config_.target_backend) {
            case Backend::VULKAN:
            case Backend::CPU:
                test_config_.slang_profile = SlangProfile::GLSL_450;
                break;
            case Backend::CUDA:
                test_config_.slang_profile = SlangProfile::SM_7_5;
                break;
            case Backend::DX12:
                test_config_.slang_profile = SlangProfile::HLSL_6_0;
                break;
        }
    }
    
    if (test_config_.slang_target == SlangTarget::AUTO) {
        switch (test_config_.target_backend) {
            case Backend::VULKAN:
            case Backend::CPU:
                test_config_.slang_target = SlangTarget::SPIRV;
                break;
            case Backend::CUDA:
                test_config_.slang_target = SlangTarget::PTX;
                break;
            case Backend::DX12:
                test_config_.slang_target = SlangTarget::HLSL;
                break;
        }
    }
}

std::string CommandLineParser::GetHelpText() const {
    std::stringstream ss;
    ss << "Kerntopia v0.1.0 - SLANG-Centric GPU Benchmarking Suite\n\n";
    ss << "Usage:\n";
    ss << "  kerntopia info [--verbose]                           Show system information\n";
    ss << "  kerntopia run <kernel> [options]                     Run specific kernel\n";
    ss << "  kerntopia run all [options]                          Run all kernels\n\n";
    ss << "Options:\n";
    ss << "  --backend, -b <backend>     GPU backend (cuda, vulkan, cpu, dx12)\n";
    ss << "  --profile, -p <profile>     SLANG profile (glsl_450, sm_7_5, sm_8_9, hlsl_6_0)\n";
    ss << "  --target, -t <target>       Compilation target (spirv, ptx, glsl, hlsl)\n";
    ss << "  --mode, -m <mode>           Test mode (functional, performance)\n";
    ss << "  --verbose, -v               Verbose output\n\n";
    ss << "Examples:\n";
    ss << "  kerntopia info --verbose\n";
    ss << "  kerntopia run conv2d --backend vulkan --profile glsl_450 --target spirv\n";
    ss << "  kerntopia run all --backend cuda --profile sm_7_5 --target ptx\n\n";
    ss << "Available kernels: vector_add, conv2d, bilateral_filter, parallel_reduction, matrix_transpose\n";
    return ss.str();
}

} // namespace kerntopia