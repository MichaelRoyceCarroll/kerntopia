#include "core/common/logger.hpp"
#include "core/common/error_handling.hpp"
#include "core/backend/backend_factory.hpp"

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
    std::cout << "System Information\n";
    std::cout << "==================\n\n";
    
    // Initialize backend factory
    auto init_result = BackendFactory::Initialize();
    if (!init_result) {
        std::cerr << "Error: Failed to initialize backend system\n";
        return;
    }
    
    // Get available backends
    auto backends = BackendFactory::GetAvailableBackends();
    auto backend_info_map = BackendFactory::GetBackendInfo();
    
    std::cout << "Available Backends: " << backends.size() << "\n";
    for (Backend backend : backends) {
        auto info_it = backend_info_map.find(backend);
        if (info_it != backend_info_map.end()) {
            const auto& info = info_it->second;
            std::cout << "  • " << info.name;
            if (!info.version.empty()) {
                std::cout << " (v" << info.version << ")";
            }
            std::cout << "\n";
            
            if (verbose) {
                std::cout << "    Library: " << info.library_path << "\n";
                if (!info.checksum.empty()) {
                    std::cout << "    Checksum: " << info.checksum.substr(0, 16) << "...\n";
                }
                std::cout << "    File Size: " << info.file_size << " bytes\n";
                if (!info.last_modified.empty()) {
                    std::cout << "    Modified: " << info.last_modified << "\n";
                }
                
                // Show devices for this backend
                auto devices_result = BackendFactory::GetDevices(backend);
                if (devices_result) {
                    const auto& devices = *devices_result;
                    std::cout << "    Devices: " << devices.size() << "\n";
                    for (size_t i = 0; i < devices.size(); ++i) {
                        const auto& device = devices[i];
                        std::cout << "      [" << i << "] " << device.name;
                        if (device.total_memory_bytes > 0) {
                            double memory_gb = static_cast<double>(device.total_memory_bytes) / (1024.0 * 1024.0 * 1024.0);
                            std::cout << " (" << std::fixed << std::setprecision(1) << memory_gb << " GB)";
                        }
                        std::cout << "\n";
                    }
                }
                std::cout << "\n";
            }
        }
    }
    
    // Show unavailable backends
    if (verbose) {
        std::cout << "\nUnavailable Backends:\n";
        for (const auto& [backend, info] : backend_info_map) {
            if (!info.available) {
                std::cout << "  • " << info.name << " - " << info.error_message << "\n";
            }
        }
    }
    
    std::cout << "\nFor detailed backend information, use: kerntopia info --verbose\n";
    
    BackendFactory::Shutdown();
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