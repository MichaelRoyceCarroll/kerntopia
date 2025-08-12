#include "core/common/logger.hpp"
#include "core/common/error_handling.hpp"
#include "core/system/system_info_service.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

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
            std::cout << "Test execution not yet implemented.\n";
            std::cout << "Requested tests: " << tests << "\n";
            std::cout << "This will be implemented in Phase 3 of development.\n";
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