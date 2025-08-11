#pragma once

#include "core/common/test_params.hpp"
#include <vector>
#include <string>

namespace kerntopia {

/**
 * @brief Command line argument parsing
 * 
 * Placeholder for Phase 2 implementation
 */
class CommandLineParser {
public:
    CommandLineParser() = default;
    
    bool Parse(int argc, char* argv[]);
    
    SuiteConfiguration GetSuiteConfig() const { return suite_config_; }
    std::vector<std::string> GetTestNames() const { return test_names_; }

private:
    SuiteConfiguration suite_config_;
    std::vector<std::string> test_names_;
};

} // namespace kerntopia