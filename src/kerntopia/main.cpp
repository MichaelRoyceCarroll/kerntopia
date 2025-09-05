#include "core/common/logger.hpp"
#include "core/common/error_handling.hpp"
#include "core/system/system_info_service.hpp"
#include "core/backend/backend_factory.hpp"
#include "command_line.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <gtest/gtest.h>

using namespace kerntopia;

// Test libraries are explicitly linked via CMakeLists.txt
// No forced linking needed - GTest static registration works automatically

/**
 * @brief Print application banner and version information
 */
void PrintBanner() {
    std::cout << "Kerntopia v0.1.0 - SLANG-Centric Kernel Execution Suite\n";
    std::cout << "Explore compute kernels with abstracted backend selection, sandboxing, benchmarking, and more.\n";
}

/**
 * @brief Print comprehensive help information
 */
void PrintHelp() {
    CommandLineParser parser;
    std::cout << parser.GetHelpText();
}

/**
 * @brief Show system information including available backends and devices
 */
void ShowSystemInfo(bool verbose) {
    SystemInfoService::ShowSystemInfo(verbose);
}

/**
 * @brief List available tests and backends
 */
void ListAvailable() {
    std::cout << "Available Tests\n";
    std::cout << "===============\n\n";
    
    std::cout << "Image Processing:\n";
    std::cout << "  ✅ conv2d       - 2D Convolution with configurable kernels [IMPLEMENTED]\n";
    std::cout << "  ⚠️  bilateral    - Edge-preserving bilateral filter [PLACEHOLDER - Not Implemented]\n\n";
    
    std::cout << "Linear Algebra:\n";
    std::cout << "  ⚠️  reduction    - Parallel reduction (sum/max/min) [PLACEHOLDER - Not Implemented]\n";
    std::cout << "  ⚠️  transpose    - Matrix transpose with memory coalescing [PLACEHOLDER - Not Implemented]\n\n";
    
    std::cout << "Examples:\n";
    std::cout << "  ⚠️  vector_add   - Template for adding new kernels [PLACEHOLDER - Not Implemented]\n\n";
    
    std::cout << "Currently only 'conv2d' is fully implemented and ready for testing.\n";
    std::cout << "Run 'kerntopia info' to see available backends and devices.\n";
}

/**
 * @brief Run tests using basic GTest integration
 * 
 * @param test_names List of test names to run
 * @param config Test configuration from command line
 * @return True if all tests passed
 */
bool RunTestsBasic(const std::vector<std::string>& test_names, const TestConfiguration& config, bool verbose = false, bool device_specified = false, bool backend_specified = true) {
    if (!backend_specified) {
        std::cout << "Running Kerntopia tests with configuration:\n";
        std::cout << "  Backend: ALL AVAILABLE BACKENDS\n";
        std::cout << "  Mode: " << config.GetModeName() << "\n";
        std::cout << "  Compilation: " << config.GetCompilationModeName() << "\n\n";
        
        // Show which backends will be tested
        auto available_backends = BackendFactory::GetAvailableBackends();
        std::cout << "Available backends: ";
        for (size_t i = 0; i < available_backends.size(); ++i) {
            if (i > 0) std::cout << ", ";
            TestConfiguration temp_config;
            temp_config.target_backend = available_backends[i];
            std::cout << temp_config.GetBackendName();
        }
        std::cout << "\n\n";
    } else {
        std::cout << "Running Kerntopia tests with configuration:\n";
        std::cout << "  Backend: " << config.GetBackendName() << "\n";
        std::cout << "  Profile: " << config.GetSlangProfileName() << "\n";
        std::cout << "  Target: " << config.GetSlangTargetName() << "\n";
        std::cout << "  Mode: " << config.GetModeName() << "\n";
        std::cout << "  Compilation: " << config.GetCompilationModeName() << "\n\n";
        
        // Check backend availability only when backend is explicitly specified
        if (!BackendFactory::IsBackendAvailable(config.target_backend)) {
            std::cerr << "Error: Backend " << config.GetBackendName() << " is not available on this system\n";
            
            // Show available backends
            auto available_backends = BackendFactory::GetAvailableBackends();
            if (!available_backends.empty()) {
                std::cerr << "Available backends: ";
                for (size_t i = 0; i < available_backends.size(); ++i) {
                    if (i > 0) std::cerr << ", ";
                    TestConfiguration temp_config;
                    temp_config.target_backend = available_backends[i];
                    std::cerr << temp_config.GetBackendName();
                }
                std::cerr << "\n";
            }
            return false;
        }
    }
    
    // Create GTest filter based on test names
    std::string gtest_filter = "";
    if (test_names.size() == 1 && test_names[0] == "all") {
        // Run all tests
        gtest_filter = "*";
    } else {
        // Build filter for specific tests
        for (size_t i = 0; i < test_names.size(); ++i) {
            if (i > 0) gtest_filter += ":";
            
            // Map test names to GTest patterns
            if (test_names[i] == "conv2d") {
                gtest_filter += "*Conv2D*";
            } else if (test_names[i] == "vector_add") {
                gtest_filter += "*VectorAdd*";
            } else if (test_names[i] == "bilateral") {
                gtest_filter += "*Bilateral*";
            } else if (test_names[i] == "reduction") {
                gtest_filter += "*Reduction*";
            } else if (test_names[i] == "transpose") {
                gtest_filter += "*Transpose*";
            } else {
                // Generic test name matching
                gtest_filter += "*" + test_names[i] + "*";
            }
        }
    }
    
    // Debug: Try running without any filter first to see what tests exist
    if (verbose) {
        std::cout << "Debug: Running with filter '*' to discover all tests...\n";
        gtest_filter = "*";
    }
    
    // Apply backend filtering only if backend was explicitly specified
    if (backend_specified) {
        std::string backend_suffix = "";
        switch (config.target_backend) {
            case Backend::CUDA:
                backend_suffix = "/CUDA";
                break;
            case Backend::VULKAN:
                backend_suffix = "/VULKAN";
                break;
            case Backend::CPU:
                backend_suffix = "/CPU";
                break;
            case Backend::DX12:
                backend_suffix = "/DX12";
                break;
        }
        
        if (!backend_suffix.empty()) {
            // Apply backend filtering based on actual test naming patterns
            // Parameterized tests use underscore-separated naming: *CUDA_*
            std::string backend_name = backend_suffix.substr(1); // Remove leading "/"
            gtest_filter = "*" + backend_name + "*";
            
            // Add device filtering if device was explicitly specified via --device flag
            // Test names include device ID like: CUDA_CUDA_SM_7_0_D0, CUDA_CUDA_SM_7_0_D1
            // Future: Dynamic device detection will replace hardcoded device IDs
            if (device_specified) {
                gtest_filter += "D" + std::to_string(config.device_id) + "*";
            }
        }
    }
    // If no backend specified, keep the original filter (all tests from all backends)
    
    std::cout << "Running tests with filter: " << gtest_filter << "\n";
    
    // Add debug output in verbose mode
    if (verbose) {
        std::cout << "Debug: Test names: ";
        for (const auto& name : test_names) {
            std::cout << name << " ";
        }
        std::cout << "\nDebug: Backend: " << config.GetBackendName() << "\n";
        std::cout << "Debug: GTest filter constructed: " << gtest_filter << "\n";
    }
    std::cout << "\n";
    
    // Initialize GTest with proper filter
    std::string gtest_filter_arg = "--gtest_filter=" + gtest_filter;
    int gtest_argc = 2;
    std::vector<const char*> gtest_argv_vec = {
        "kerntopia",
        gtest_filter_arg.c_str(),
        nullptr  // Null terminate for safety
    };
    
    const char** gtest_argv = gtest_argv_vec.data();
    
    // Initialize GTest with our arguments
    ::testing::InitGoogleTest(&gtest_argc, const_cast<char**>(gtest_argv));
    
    // Set up test environment - this will be used by tests to get configuration
    // Note: In a more complete implementation, we'd use a proper test environment
    
    // Run tests
    int result = RUN_ALL_TESTS();
    
    // Get test results to check if any tests actually ran
    const auto& unit_test = *::testing::UnitTest::GetInstance();
    int total_tests = unit_test.total_test_count();
    int successful_tests = unit_test.successful_test_count();
    int failed_tests = unit_test.failed_test_count();
    
    if (total_tests == 0) {
        std::cout << "\n⚠️  No tests matched the filter: " << gtest_filter << "\n";
        std::cout << "Try running 'kerntopia run all' or check available tests with '--gtest_list_tests'\n";
        return false;
    } else if (result == 0) {
        std::cout << "\n✅ All " << successful_tests << " tests passed!\n";
        return true;
    } else {
        std::cout << "\n❌ " << failed_tests << " of " << total_tests << " tests failed.\n";
        return false;
    }
}

/**
 * @brief Run in pure GTest mode - bypass all Kerntopia command logic
 */
int RunPureGTestMode(int argc, char* argv[]) {
    std::cout << "Running in GTest mode...\n\n";
    
    // Initialize logging with same default configuration as regular mode (clean output)
    Logger::Config log_config;
    log_config.min_level = LogLevel::WARNING;  // Match regular mode default
    log_config.log_to_console = true;
    Logger::Initialize(log_config);
    
    // Initialize backend factory for tests
    auto init_result = BackendFactory::Initialize();
    if (!init_result.HasValue()) {
        std::cerr << "Error: Failed to initialize backends: " << init_result.GetError().message << "\n";
        return 1;
    }
    
    // Initialize GTest with all provided arguments
    ::testing::InitGoogleTest(&argc, argv);
    
    // Run tests
    int result = RUN_ALL_TESTS();
    
    // Cleanup
    BackendFactory::Shutdown();
    Logger::Shutdown();
    
    return result;
}

/**
 * @brief Parse command line arguments
 */
int main(int argc, char* argv[]) {
    PrintBanner();
    
    // Early detection: Check for any GTest arguments
    bool has_gtest_args = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("--gtest_") == 0) {
            has_gtest_args = true;
            break;
        }
    }
    
    // If GTest arguments detected, switch to pure GTest mode
    if (has_gtest_args) {
        return RunPureGTestMode(argc, argv);
    }
    
    if (argc < 2) {
        PrintHelp();
        return 1;
    }
    
    // Initialize logging with new default configuration (clean output)
    Logger::Config log_config;
    log_config.min_level = LogLevel::WARNING;  // Changed from INFO to WARNING
    log_config.log_to_console = true;
    Logger::Initialize(log_config);
    
    std::string command = argv[1];
    
    try {
        if (command == "help" || command == "--help" || command == "-h") {
            PrintHelp();
        }
        else if (command == "info") {
            bool verbose = false;
            bool help_requested = false;
            for (int i = 2; i < argc; ++i) {
                std::string arg = argv[i];
                if (arg == "--verbose" || arg == "-v") {
                    verbose = true;
                } else if (arg == "--help" || arg == "-h") {
                    help_requested = true;
                }
            }
            
            if (help_requested) {
                CommandLineParser parser;
                std::cout << parser.GetInfoHelpText();
            } else {
                ShowSystemInfo(verbose);
            }
        }
        else if (command == "list") {
            bool help_requested = false;
            for (int i = 2; i < argc; ++i) {
                std::string arg = argv[i];
                if (arg == "--help" || arg == "-h") {
                    help_requested = true;
                    break;
                }
            }
            
            if (help_requested) {
                CommandLineParser parser;
                std::cout << parser.GetListHelpText();
            } else {
                ListAvailable();
            }
        }
        else if (command == "run") {
            // Check for help first, even if no test specified
            bool help_requested = false;
            for (int i = 2; i < argc; ++i) {
                std::string arg = argv[i];
                if (arg == "--help" || arg == "-h") {
                    help_requested = true;
                    break;
                }
            }
            
            if (help_requested) {
                CommandLineParser parser;
                std::cout << parser.GetRunHelpText();
                return 0;
            }
            
            if (argc < 3) {
                std::cerr << "Error: No tests specified\n";
                CommandLineParser parser;
                std::cout << parser.GetRunHelpText();
                return 1;
            }
            
            std::string tests = argv[2];
            
            // Parse command line arguments using CommandLineParser
            CommandLineParser parser;
            if (!parser.Parse(argc, argv)) {
                std::cerr << "Error: Failed to parse command line arguments\n";
                std::cerr << parser.GetHelpText();
                return 1;
            }
            
            // Get parsed configuration
            auto test_config = parser.GetTestConfig();
            auto test_names = parser.GetTestNames();
            
            // Apply user-specified log levels (override default)
            if (parser.IsLogLevelsSpecified()) {
                auto levels = parser.GetLogLevels();
                
                if (levels.count(-1)) {
                    // Level -1: Complete silence
                    log_config.log_to_console = false;
                    Logger::GetInstance().SetConsoleOutput(false);
                } else {
                    // Determine minimum level from specified levels
                    // Level 0: WARNING+ (normal/default)
                    // Level 1: INFO+ 
                    // Level 2: DEBUG+
                    LogLevel min_level = LogLevel::WARNING; // fallback default
                    if (levels.count(2)) {
                        min_level = LogLevel::DBG;
                    } else if (levels.count(1)) {
                        min_level = LogLevel::INFO;
                    } else if (levels.count(0)) {
                        min_level = LogLevel::WARNING;
                    }
                    
                    Logger::GetInstance().SetLogLevel(min_level);
                }
            }
            
            // Initialize backend factory
            auto init_result = BackendFactory::Initialize();
            if (!init_result.HasValue()) {
                std::cerr << "Error: Failed to initialize backends: " << init_result.GetError().message << "\n";
                return 1;
            }
            
            // Run tests using basic GTest integration
            auto result = RunTestsBasic(test_names, test_config, parser.IsVerbose(), parser.IsDeviceSpecified(), parser.IsBackendSpecified());
            
            // Cleanup
            BackendFactory::Shutdown();
            
            return result ? 0 : 1;
        }
        else {
            std::cerr << "Error: Unknown command '" << command << "'\n";
            PrintHelp();
            return 1;
        }
    }
    catch (const KerntopiaException& e) {
        std::cerr << "Error: " << ErrorHandler::FormatError(e.GetErrorInfo()) << "\n";
        Logger::Shutdown();
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << "\n";
        Logger::Shutdown();
        return 1;
    }
    
    Logger::Shutdown();
    return 0;
}