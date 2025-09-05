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
    
    ss << "USAGE:\n";
    ss << "  kerntopia <command> [options]\n";
    ss << "  kerntopia --gtest_<option>              # Direct GTest mode\n\n";
    
    ss << "COMMANDS:\n";
    ss << "  info [--verbose]                        Show system information\n";
    ss << "  run <kernel|all> [options]              Run tests\n";
    ss << "  list                                    List available tests\n";
    ss << "  help                                    Show this help\n\n";
    
    ss << "  Use '<command> --help' for command-specific help\n\n";
    
    ss << "RUN OPTIONS:\n";
    ss << "  --backend, -b <name>        GPU backend: cuda, vulkan, cpu\n";
    ss << "  --device, -d <id>           Target device ID (use after --backend)\n";
    ss << "  --profile, -p <profile>     SLANG profile:\n";
    ss << "                                cuda: cuda_sm_6_0, cuda_sm_7_0, cuda_sm_8_0\n";
    ss << "                                vulkan: glsl_450\n";
    ss << "  --target, -t <target>       Compilation target: spirv, ptx, glsl, hlsl\n";
    ss << "  --mode, -m <mode>           Test mode: functional, performance\n";
    ss << "  --jit                       Use just-in-time compilation (NOT IMPLEMENTED)\n";
    ss << "  --precompiled               Use precompiled kernels (default)\n\n";
    
    ss << "GLOBAL OPTIONS:\n";
    ss << "  --verbose, -v               Verbose output\n";
    ss << "  --logger, --log <levels>    Logging control:\n";
    ss << "                                -1=silent, 0=normal, 1=info, 2=debug\n";
    ss << "                                Words: normal, info, debug\n";
    ss << "                                Comma-separated: 1,2 or info,debug\n\n";
    
    ss << "GTEST INTEGRATION:\n";
    ss << "  --gtest_list_tests          List all available GTest tests\n";
    ss << "  --gtest_filter=<pattern>    Run tests matching pattern\n";
    ss << "  --gtest_help                Show GTest options\n\n";
    
    ss << "EXAMPLES:\n";
    ss << "  # Basic usage\n";
    ss << "  kerntopia info --verbose\n";
    ss << "  kerntopia list\n";
    ss << "  kerntopia run conv2d\n\n";
    
    ss << "  # Backend selection\n";
    ss << "  kerntopia run conv2d --backend vulkan\n";
    ss << "  kerntopia run conv2d --backend cuda --device 0\n\n";
    
    ss << "  # Advanced configuration\n";
    ss << "  kerntopia run conv2d --backend cuda --profile cuda_sm_7_0 --target ptx\n";
    ss << "  kerntopia run all --backend vulkan --mode performance --logger info\n\n";
    
    ss << "  # GTest mode for advanced filtering\n";
    ss << "  kerntopia --gtest_list_tests\n";
    ss << "  kerntopia --gtest_filter=\"*CUDA*\"\n";
    ss << "  kerntopia --gtest_filter=\"*Conv2D*VULKAN*D0*\"\n\n";
    
    ss << "TEST STATUS:\n";
    ss << "  ✅ conv2d           - 2D Convolution (IMPLEMENTED)\n";
    ss << "  🚧 vector_add       - Vector addition (PLACEHOLDER)\n";
    ss << "  🚧 bilateral_filter - Bilateral filter (PLACEHOLDER)\n";
    ss << "  🚧 reduction        - Parallel reduction (PLACEHOLDER)\n";
    ss << "  🚧 transpose        - Matrix transpose (PLACEHOLDER)\n\n";
    
    ss << "For detailed test information: kerntopia list\n";
    ss << "For system capabilities: kerntopia info\n";
    
    return ss.str();
}

std::string CommandLineParser::GetInfoHelpText() const {
    std::stringstream ss;
    ss << "kerntopia info - Show system information\n\n";
    ss << "USAGE:\n";
    ss << "  kerntopia info [--verbose]\n\n";
    ss << "OPTIONS:\n";
    ss << "  --verbose, -v    Show detailed system information including:\n";
    ss << "                   - All detected backends and capabilities\n";
    ss << "                   - Device specifications and memory\n";
    ss << "                   - Driver versions and library paths\n";
    ss << "                   - SLANG compilation targets\n\n";
    ss << "EXAMPLES:\n";
    ss << "  kerntopia info              # Basic system summary\n";
    ss << "  kerntopia info --verbose    # Detailed information\n";
    return ss.str();
}

std::string CommandLineParser::GetRunHelpText() const {
    std::stringstream ss;
    ss << "kerntopia run - Execute GPU compute kernels\n\n";
    ss << "USAGE:\n";
    ss << "  kerntopia run <kernel|all> [options]\n\n";
    ss << "KERNELS:\n";
    ss << "  conv2d         2D convolution kernel (IMPLEMENTED)\n";
    ss << "  all            Run all implemented kernels\n\n";
    ss << "OPTIONS:\n";
    ss << "  --backend, -b <name>     Choose backend: cuda, vulkan, cpu\n";
    ss << "  --device, -d <id>        Device ID (0, 1, 2...) - use after --backend\n";
    ss << "  --profile, -p <profile>  SLANG profile for compilation\n";
    ss << "  --target, -t <target>    Output format: spirv, ptx, glsl, hlsl\n";
    ss << "  --mode, -m <mode>        Test type: functional, performance\n";
    ss << "  --jit                    Compile at runtime (NOT IMPLEMENTED)\n";
    ss << "  --logger <level>         Logging: -1=silent, 0=normal, 1=info, 2=debug\n\n";
    ss << "EXAMPLES:\n";
    ss << "  kerntopia run conv2d                                    # Use best available backend\n";
    ss << "  kerntopia run conv2d --backend vulkan                   # Force Vulkan\n";
    ss << "  kerntopia run conv2d --backend cuda --device 1          # CUDA device 1\n";
    ss << "  kerntopia run all --mode performance --logger info      # Performance testing\n\n";
    ss << "BACKEND-SPECIFIC EXAMPLES:\n";
    ss << "  # CUDA with specific compute capability\n";
    ss << "  kerntopia run conv2d --backend cuda --profile cuda_sm_7_0 --target ptx\n\n";
    ss << "  # Vulkan with SPIR-V output\n";
    ss << "  kerntopia run conv2d --backend vulkan --profile glsl_450 --target spirv\n\n";
    ss << "For GTest-style filtering, use: kerntopia --gtest_filter=\"pattern\"\n";
    return ss.str();
}

std::string CommandLineParser::GetListHelpText() const {
    std::stringstream ss;
    ss << "kerntopia list - Show available tests and backends\n\n";
    ss << "USAGE:\n";
    ss << "  kerntopia list\n\n";
    ss << "DESCRIPTION:\n";
    ss << "  Lists all kernels with implementation status, categorized by domain:\n";
    ss << "  - Image Processing: conv2d, bilateral_filter\n";
    ss << "  - Linear Algebra: reduction, transpose  \n";
    ss << "  - Examples: vector_add\n\n";
    ss << "  Shows which tests are implemented (✅) vs placeholders (🚧)\n\n";
    ss << "RELATED COMMANDS:\n";
    ss << "  kerntopia info                    # Show backend capabilities\n";
    ss << "  kerntopia --gtest_list_tests      # List all GTest test cases\n";
    ss << "  kerntopia run <kernel>            # Run specific kernel\n";
    return ss.str();
}

} // namespace kerntopia