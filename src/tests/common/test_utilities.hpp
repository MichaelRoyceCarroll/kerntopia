#pragma once

#include "core/common/test_params.hpp"
#include "core/common/kernel_result.hpp"

namespace kerntopia {

/**
 * @brief Test utility functions
 * 
 * Placeholder for Phase 3 test framework implementation
 */
namespace test_utils {
    
    /**
     * @brief Create default test configuration
     * 
     * @return Default test configuration
     */
    TestConfiguration CreateDefaultConfig();
    
    /**
     * @brief Validate kernel result
     * 
     * @param result Kernel execution result
     * @return True if result is valid
     */
    bool ValidateResult(const KernelResult& result);
    
} // namespace test_utils

} // namespace kerntopia