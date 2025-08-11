#pragma once

#include <gtest/gtest.h>
#include "core/common/test_params.hpp"
#include "core/common/kernel_result.hpp"

namespace kerntopia {

/**
 * @brief Base class for all kernel tests
 * 
 * Placeholder for Phase 3 test framework implementation
 */
class BaseKernelTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;
    
    TestConfiguration config_;
    KernelResult result_;
};

} // namespace kerntopia