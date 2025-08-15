#pragma once

#include "core/common/test_params.hpp"
#include <vector>
#include <string>

namespace kerntopia {

/**
 * @brief Command line argument parsing with SLANG profile × target support
 * 
 * Supports the following command patterns:
 * - kerntopia info [--verbose]
 * - kerntopia run <kernel> --backend <backend> --profile <profile> --target <target>
 * - kerntopia run all --backend <backend> --profile <profile> --target <target>
 * - kerntopia-<kernel> --backend <backend> --profile <profile> --target <target>
 */
class CommandLineParser {
public:
    CommandLineParser() = default;
    
    /**
     * @brief Parse command line arguments
     * 
     * @param argc Argument count
     * @param argv Argument vector
     * @return True if parsing succeeded
     */
    bool Parse(int argc, char* argv[]);
    
    /**
     * @brief Get parsed suite configuration
     */
    SuiteConfiguration GetSuiteConfig() const { return suite_config_; }
    
    /**
     * @brief Get list of test names to run
     */
    std::vector<std::string> GetTestNames() const { return test_names_; }
    
    /**
     * @brief Get parsed test configuration 
     */
    TestConfiguration GetTestConfig() const { return test_config_; }
    
    /**
     * @brief Check if info command was requested
     */
    bool IsInfoCommand() const { return info_command_; }
    
    /**
     * @brief Check if verbose mode was requested
     */
    bool IsVerbose() const { return verbose_; }
    
    /**
     * @brief Check if help was requested
     */
    bool IsHelpRequested() const { return help_requested_; }
    
    /**
     * @brief Check if device was explicitly specified
     */
    bool IsDeviceSpecified() const { return device_specified_; }
    
    /**
     * @brief Check if backend was explicitly specified
     */
    bool IsBackendSpecified() const { return backend_specified_; }
    
    /**
     * @brief Get help text
     */
    std::string GetHelpText() const;

private:
    // Parsing helper methods
    bool ParseOptions(int argc, char* argv[]);
    bool ParseBackend(const std::string& backend_str);
    bool ParseProfile(const std::string& profile_str);  
    bool ParseTarget(const std::string& target_str);
    bool ParseMode(const std::string& mode_str);
    bool ParseDevice(const std::string& device_str);
    void SetDefaultProfileTarget();
    void PrintUsage() const;
    
    // Parsed configuration
    SuiteConfiguration suite_config_;
    TestConfiguration test_config_;
    std::vector<std::string> test_names_;
    
    // Command flags
    bool info_command_ = false;
    bool verbose_ = false;
    bool help_requested_ = false;
    bool backend_specified_ = false;
    bool device_specified_ = false;
};

} // namespace kerntopia