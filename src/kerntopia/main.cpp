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
 * @brief Print usage information
 */
void PrintUsage() {
    std::cout << "Usage: kerntopia <command> [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  run <tests>     Run specified tests (or 'all' for all tests)\n";
    std::cout << "  info            Show system information\n";
    std::cout << "  list            List available tests and backends\n";
    std::cout << "  help            Show this help message\n\n";
    std::cout << "Options:\n";
    std::cout << "  --backend <name>    Target backend (cuda, vulkan, cpu)\n";
    std::cout << "  --device <id>       Target device ID\n";
    std::cout << "  --mode <mode>       Test mode (functional, performance)\n";
    std::cout << "  --verbose           Enable verbose output\n";
    std::cout << "  --log-file <path>   Log to file\n";
    std::cout << "\nExamples:\n";
    std::cout << "  kerntopia run all\n";
    std::cout << "  kerntopia run conv2d --backend cuda --mode performance\n";
    std::cout << "  kerntopia info --verbose\n";
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
    std::cout << "  • conv2d       - 2D Convolution with configurable kernels\n";
    std::cout << "  • bilateral    - Edge-preserving bilateral filter\n\n";
    
    std::cout << "Linear Algebra:\n";
    std::cout << "  • reduction    - Parallel reduction (sum/max/min)\n";
    std::cout << "  • transpose    - Matrix transpose with memory coalescing\n\n";
    
    std::cout << "Examples:\n";
    std::cout << "  • vector_add   - Template for adding new kernels\n\n";
    
    std::cout << "Run 'kerntopia info' to see available backends and devices.\n";
}

/**
 * @brief Run tests using basic GTest integration
 * 
 * @param test_names List of test names to run
 * @param config Test configuration from command line
 * @return True if all tests passed
 */
bool RunTestsBasic(const std::vector<std::string>& test_names, const TestConfiguration& config, bool verbose = false) {
    std::cout << "Running Kerntopia tests with configuration:\n";
    std::cout << "  Backend: " << config.GetBackendName() << "\n";
    std::cout << "  Profile: " << config.GetSlangProfileName() << "\n";
    std::cout << "  Target: " << config.GetSlangTargetName() << "\n";
    std::cout << "  Mode: " << config.GetModeName() << "\n";
    std::cout << "  Compilation: " << config.GetCompilationModeName() << "\n\n";
    
    // Check backend availability
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
    
    // Apply backend filtering to test names based on actual GTest naming
    std::string backend_suffix = "";
    switch (config.target_backend) {
        case Backend::CUDA:
            backend_suffix = "/CUDA";
            break;
        case Backend::VULKAN:
            backend_suffix = "/Vulkan";
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
        // Parameterized tests end with /BackendName
        gtest_filter = "*" + backend_suffix;
    }
    
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
        gtest_filter_arg.c_str()
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
 * @brief Parse command line arguments
 */
int main(int argc, char* argv[]) {
    PrintBanner();
    
    if (argc < 2) {
        PrintUsage();
        return 1;
    }
    
    // Initialize logging with default configuration
    Logger::Config log_config;
    log_config.min_level = LogLevel::INFO;
    log_config.log_to_console = true;
    Logger::Initialize(log_config);
    
    std::string command = argv[1];
    
    try {
        if (command == "help" || command == "--help" || command == "-h") {
            PrintUsage();
        }
        else if (command == "info") {
            bool verbose = false;
            for (int i = 2; i < argc; ++i) {
                if (std::string(argv[i]) == "--verbose" || std::string(argv[i]) == "-v") {
                    verbose = true;
                }
            }
            ShowSystemInfo(verbose);
        }
        else if (command == "list") {
            ListAvailable();
        }
        else if (command == "run") {
            if (argc < 3) {
                std::cerr << "Error: No tests specified\n";
                std::cerr << "Usage: kerntopia run <tests> [options]\n";
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
            
            // Initialize backend factory
            auto init_result = BackendFactory::Initialize();
            if (!init_result.HasValue()) {
                std::cerr << "Error: Failed to initialize backends: " << init_result.GetError().message << "\n";
                return 1;
            }
            
            // Run tests using basic GTest integration
            auto result = RunTestsBasic(test_names, test_config, parser.IsVerbose());
            
            // Cleanup
            BackendFactory::Shutdown();
            
            return result ? 0 : 1;
        }
        else {
            std::cerr << "Error: Unknown command '" << command << "'\n";
            PrintUsage();
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