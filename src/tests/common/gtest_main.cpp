#include <gtest/gtest.h>
#include "core/common/logger.hpp"

int main(int argc, char **argv) {
    // Initialize logging for tests
    kerntopia::Logger::Config log_config;
    log_config.min_level = kerntopia::LogLevel::INFO;
    log_config.log_to_console = true;
    kerntopia::Logger::Initialize(log_config);
    
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    
    kerntopia::Logger::Shutdown();
    return result;
}