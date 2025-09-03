#pragma once

#include "system_interrogator.hpp"

#include <iostream>
#include <iomanip>

namespace kerntopia {

/**
 * @brief System Information Display Service
 * 
 * Provides consistent system information display across all executable types:
 * - Suite mode (kerntopia)
 * - Standalone executables (kerntopia-conv2d)
 * - Python wrapper integration
 * 
 * This service abstracts the display logic from main.cpp for reusability.
 */
class SystemInfoService {
public:
    /**
     * @brief Display complete system information
     * 
     * @param verbose Enable verbose output with detailed information
     * @param stream Output stream (defaults to std::cout)
     */
    static void ShowSystemInfo(bool verbose = false, std::ostream& stream = std::cout);
    
    /**
     * @brief Display only backend information (no SLANG)
     * 
     * Useful for standalone executables that only need backend info.
     * 
     * @param verbose Enable verbose output
     * @param stream Output stream
     */
    static void ShowBackendsOnly(bool verbose = false, std::ostream& stream = std::cout);
    
    /**
     * @brief Display only SLANG information
     * 
     * @param verbose Enable verbose output
     * @param stream Output stream
     */
    static void ShowSlangOnly(bool verbose = false, std::ostream& stream = std::cout);
    
    /**
     * @brief Get system information as structured data
     * 
     * Returns the raw SystemInfo structure for programmatic access.
     * Useful for Python wrapper serialization.
     * 
     * @return SystemInfo structure or error
     */
    static Result<SystemInfo> GetSystemInformation();

private:
    // Display helper methods
    static void DisplayRuntimeInfo(const RuntimeInfo& runtime, const std::string& runtime_name, 
                                  bool verbose, std::ostream& stream);
    static void DisplaySlangInfo(const RuntimeInfo& slang, bool verbose, std::ostream& stream);
    static void DisplayDevicesFromSystemInfo(const std::vector<DeviceInfo>& devices, std::ostream& stream);
    static void DisplayUnavailableBackends(const SystemInfo& system_info, bool verbose, std::ostream& stream);
};

} // namespace kerntopia