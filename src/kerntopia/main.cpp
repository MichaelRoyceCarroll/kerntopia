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

/**
 * @brief Print application banner and version information
 */
void PrintBanner() {
    std::cout << "Kerntopia v0.1.0 - SLANG-Centric GPU Benchmarking Suite\n";
    std::cout << "Educational GPU compute kernels for learning and benchmarking\n";
    std::cout << "Target Audience: GPU developers, SIGGRAPH attendees, GPU computing enthusiasts\n\n";
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
bool RunTestsBasic(const std::vector<std::string>& test_names, const TestConfiguration& config) {
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
    
    // Set up GTest environment variables based on configuration
    std::string backend_filter = "";
    switch (config.target_backend) {
        case Backend::CUDA:
            backend_filter = "*CUDA*";
            break;
        case Backend::VULKAN:
            backend_filter = "*Vulkan*";
            break;
        case Backend::CPU:
            backend_filter = "*CPU*";
            break;
        default:
            backend_filter = "*";
            break;
    }
    
    // Combine test and backend filters
    if (gtest_filter != "*" && backend_filter != "*") {
        gtest_filter = "(" + gtest_filter + "):(" + backend_filter + ")";
    } else if (backend_filter != "*") {
        gtest_filter = backend_filter;
    }
    
    std::cout << "Running tests with filter: " << gtest_filter << "\n\n";
    
    // Initialize GTest
    int gtest_argc = 3;
    const char* gtest_argv[] = {
        "kerntopia",
        "--gtest_filter",
        gtest_filter.c_str()
    };
    
    // Initialize GTest with our arguments
    ::testing::InitGoogleTest(&gtest_argc, const_cast<char**>(gtest_argv));
    
    // Set up test environment - this will be used by tests to get configuration
    // Note: In a more complete implementation, we'd use a proper test environment
    
    // Run tests
    int result = RUN_ALL_TESTS();
    
    if (result == 0) {
        std::cout << "\n✅ All tests passed!\n";
        return true;
    } else {
        std::cout << "\n❌ Some tests failed.\n";
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
            auto result = RunTestsBasic(test_names, test_config);
            
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