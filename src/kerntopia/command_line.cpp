#include "command_line.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

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
            test_names_ = {"conv2d"}; // Only implemented test
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
        else if (arg == "--jit") {
            test_config_.compilation_mode = CompilationMode::JIT;
        }
        else if (arg == "--precompiled") {
            test_config_.compilation_mode = CompilationMode::PRECOMPILED;
        }
        else if (arg == "--device" || arg == "-d") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --device requires argument\n";
                return false;
            }
            if (!ParseDevice(argv[++i])) return false;
        }
        else if (arg == "--verbose" || arg == "-v") {
            verbose_ = true;
        }
        else if (arg == "--logger" || arg == "--log" || arg == "--log-level") {
            if (i + 1 >= argc) {
                // Default to level 0 (WARNING+) if no argument provided
                log_levels_.insert(0);
            } else {
                std::string value = argv[++i];
                ParseLogLevels(value);
            }
            log_levels_specified_ = true;
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
    backend_specified_ = true;
    return true;
}

bool CommandLineParser::ParseProfile(const std::string& profile_str) {
    if (profile_str == "glsl_450") {
        test_config_.slang_profile = SlangProfile::GLSL_450;
    } else if (profile_str == "cuda_sm_6_0") {
        test_config_.slang_profile = SlangProfile::CUDA_SM_6_0;
    } else if (profile_str == "cuda_sm_7_0") {
        test_config_.slang_profile = SlangProfile::CUDA_SM_7_0;
    } else if (profile_str == "cuda_sm_8_0") {
        test_config_.slang_profile = SlangProfile::CUDA_SM_8_0;
    } else if (profile_str == "hlsl_6_0") {
        test_config_.slang_profile = SlangProfile::HLSL_6_0;
    } else {
        std::cerr << "Error: Unknown profile '" << profile_str << "'. Valid options: glsl_450, cuda_sm_6_0, cuda_sm_7_0, cuda_sm_8_0, hlsl_6_0\n";
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

bool CommandLineParser::ParseDevice(const std::string& device_str) {
    // Check if backend has been specified first
    if (!backend_specified_) {
        std::cerr << "Error: --device can only be used after --backend is specified\n";
        return false;
    }
    
    // Parse device ID
    try {
        int device_id = std::stoi(device_str);
        if (device_id < 0) {
            std::cerr << "Error: Device ID must be non-negative\n";
            return false;
        }
        test_config_.device_id = device_id;
        device_specified_ = true;
    } catch (const std::exception& e) {
        std::cerr << "Error: Invalid device ID '" << device_str << "'. Must be a non-negative integer\n";
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
                test_config_.slang_profile = SlangProfile::CUDA_SM_7_0;
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

void CommandLineParser::ParseLogLevels(const std::string& value) {
    // Handle comma separation: "1,2" or "info,debug"
    std::stringstream ss(value);
    std::string token;
    while (std::getline(ss, token, ',')) {
        // Trim whitespace
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        
        int level = ParseSingleLogLevel(token);
        if (level >= -1 && level <= 2) {
            log_levels_.insert(level);
        } else {
            std::cerr << "Warning: Invalid log level '" << token << "'. Valid: -1,0,1,2 or normal,info,debug\\n";
        }
    }
    
    // If no valid levels were parsed, default to level 0
    if (log_levels_.empty()) {
        log_levels_.insert(0);
    }
}

int CommandLineParser::ParseSingleLogLevel(const std::string& token) {
    // Try numeric first (including negative)
    if (!token.empty() && (std::isdigit(token[0]) || token[0] == '-')) {
        try {
            return std::stoi(token);
        } catch (const std::exception&) {
            return 0; // Default to normal on parse error
        }
    }
    
    // Handle word alternatives (case-insensitive)
    std::string lower_token = token;
    std::transform(lower_token.begin(), lower_token.end(), lower_token.begin(), ::tolower);
    
    if (lower_token == "normal") return 0;
    if (lower_token == "info") return 1;
    if (lower_token == "debug" || lower_token == "dbg") return 2;
    
    return 0; // Default to normal
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
    ss << "  --device, -d <id>           Target device ID (use after --backend)\n";
    ss << "  --profile, -p <profile>     SLANG profile (glsl_450, cuda_sm_6_0, cuda_sm_7_0, cuda_sm_8_0, hlsl_6_0)\n";
    ss << "  --target, -t <target>       Compilation target (spirv, ptx, glsl, hlsl)\n";
    ss << "  --mode, -m <mode>           Test mode (functional, performance)\n";
    ss << "  --jit                       Use just-in-time compilation\n";
    ss << "  --precompiled               Use precompiled kernels (default)\n";
    ss << "  --verbose, -v               Verbose output\n";
    ss << "  --logger, --log, --log-level <levels>   Logging control:\n";
    ss << "                                -1=silent, 0=normal (default), 1=info, 2=debug\n";
    ss << "                                Words: normal, info, debug/dbg\n";
    ss << "                                Comma-separated: 1,2 or info,debug\n\n";
    ss << "Examples:\n";
    ss << "  kerntopia info --verbose\n";
    ss << "  kerntopia run conv2d --backend vulkan --device 0 --profile glsl_450 --target spirv\n";
    ss << "  kerntopia run conv2d --backend cuda --device 0 --profile cuda_sm_7_0 --target ptx --jit\n\n";
    ss << "Currently implemented: conv2d\n";
    ss << "Planned kernels: vector_add, bilateral_filter, parallel_reduction, matrix_transpose\n";
    return ss.str();
}

} // namespace kerntopia