#pragma once

#include <gtest/gtest.h>
#include "core/common/test_params.hpp"
#include "core/common/kernel_result.hpp"
#include "core/backend/backend_factory.hpp"
#include "core/backend/ikernel_runner.hpp"
#include "core/imaging/image_loader.hpp"
#include "core/imaging/image_data.hpp"
#include "core/common/logger.hpp"
#include <memory>
#include <vector>
#include <filesystem>

namespace kerntopia {

/**
 * @brief Base class for all kernel tests with GTest integration
 * 
 * Provides comprehensive testing infrastructure including:
 * - Backend initialization and cleanup
 * - Image loading and validation utilities  
 * - Statistical analysis for performance tests
 * - Parameterized test support
 * - Output validation and comparison
 */
class BaseKernelTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;
    
    /**
     * @brief Configure test parameters before execution
     * Override in derived classes to customize configuration
     */
    virtual void ConfigureTest() {}
    
    /**
     * @brief Load input data for the test
     * Override in derived classes to load specific test data
     */
    virtual Result<void> LoadInputData() { return KERNTOPIA_VOID_SUCCESS(); }
    
    /**
     * @brief Execute the kernel with current configuration
     * 
     * @return Execution result
     */
    virtual Result<KernelResult> ExecuteKernel() = 0;
    
    /**
     * @brief Validate kernel output
     * Override in derived classes for specific validation logic
     * 
     * @param result Kernel execution result
     * @return Validation results
     */
    virtual ValidationResults ValidateOutput(const KernelResult& result);
    
    /**
     * @brief Run single functional test iteration
     * 
     * @return Test result
     */
    Result<KernelResult> RunFunctionalTest();
    
    /**
     * @brief Run multiple iterations for performance analysis
     * 
     * @param iterations Number of iterations
     * @return Statistical summary of results
     */
    Result<StatisticalSummary> RunPerformanceTest(int iterations);
    
    /**
     * @brief Load image from file
     * 
     * @param path Image file path
     * @param format Expected image format
     * @return Loaded image data or error
     */
    Result<ImageData> LoadImage(const std::string& path, ImageFormat format);
    
    /**
     * @brief Save image to file
     * 
     * @param image Image data to save
     * @param path Output file path
     * @return Success result
     */
    Result<void> SaveImage(const ImageData& image, const std::string& path);
    
    /**
     * @brief Compare two images with tolerance
     * 
     * @param expected Expected image
     * @param actual Actual image  
     * @param tolerance Comparison tolerance
     * @return Validation results
     */
    ValidationResults CompareImages(const ImageData& expected, const ImageData& actual, float tolerance);
    
    /**
     * @brief Calculate statistical summary from multiple results
     * 
     * @param results Vector of kernel results
     * @return Statistical summary
     */
    StatisticalSummary CalculateStatistics(const std::vector<KernelResult>& results);
    
    /**
     * @brief Create output directory if it doesn't exist
     * 
     * @param path Directory path
     * @return Success result
     */
    Result<void> CreateOutputDirectory(const std::string& path);
    
    /**
     * @brief Generate unique filename with timestamp
     * 
     * @param base_name Base filename
     * @param extension File extension
     * @return Unique filename
     */
    std::string GenerateUniqueFilename(const std::string& base_name, const std::string& extension);
    
    /**
     * @brief Check if backend is available for testing
     * 
     * @param backend Backend to check
     * @return True if available
     */
    bool IsBackendAvailable(Backend backend);
    
    /**
     * @brief Initialize backend infrastructure
     * 
     * @return Success result
     */
    Result<void> InitializeBackend();
    
    /**
     * @brief Shutdown backend infrastructure
     */
    void ShutdownBackend();
    
    /**
     * @brief Get test data directory path
     * 
     * @param kernel_name Kernel name
     * @return Test data directory path
     */
    std::string GetTestDataPath(const std::string& kernel_name);
    
    /**
     * @brief Get precompiled kernel path
     * 
     * @param kernel_name Kernel name
     * @return Compiled kernel file path
     */
    std::string GetKernelPath(const std::string& kernel_name);

protected:
    // Test configuration and state
    TestConfiguration config_;
    KernelResult result_;
    
    // Backend infrastructure
    std::unique_ptr<IKernelRunner> kernel_runner_;
    
    // Test data management
    std::unique_ptr<ImageLoader> image_loader_;
    std::string test_data_dir_;
    std::string output_dir_;
    
    // Test metadata
    std::string test_name_;
    std::chrono::system_clock::time_point test_start_time_;
};

/**
 * @brief Parameterized test base for multi-backend testing
 * 
 * Use this base class for tests that should run across multiple backends
 */
class ParameterizedKernelTest : public BaseKernelTest, 
                               public ::testing::WithParamInterface<Backend> {
protected:
    void SetUp() override;
    
    /**
     * @brief Get current test backend from parameters
     */
    Backend GetTestBackend() const { return GetParam(); }
};

/**
 * @brief Test fixture for image processing kernels
 */
class ImageKernelTest : public BaseKernelTest {
protected:
    void SetUp() override;
    
    /**
     * @brief Load test images for processing
     * 
     * @param input_name Input image filename
     * @param reference_name Reference output filename (optional)
     * @return Success result
     */
    Result<void> LoadTestImages(const std::string& input_name, 
                               const std::string& reference_name = "");
    
    /**
     * @brief Validate image processing output
     * 
     * @param result Kernel execution result
     * @return Validation results
     */
    ValidationResults ValidateOutput(const KernelResult& result) override;

protected:
    ImageData input_image_;
    ImageData reference_image_;
    ImageData output_image_;
    bool has_reference_ = false;
};

// Helper macros for common test patterns

/**
 * @brief Define a parameterized test across all available backends
 */
#define KERNTOPIA_TEST_ALL_BACKENDS(test_class, test_name) \
    TEST_P(test_class, test_name) { \
        if (!IsBackendAvailable(GetTestBackend())) { \
            GTEST_SKIP() << "Backend " << config_.GetBackendName() << " not available"; \
        } \
        auto result = RunFunctionalTest(); \
        ASSERT_TRUE(result.HasValue()) << "Test execution failed: " << result.GetError().message; \
        EXPECT_TRUE(result->IsValid()) << "Kernel validation failed"; \
    } \
    INSTANTIATE_TEST_SUITE_P(AllBackends, test_class, \
        ::testing::Values(Backend::CUDA, Backend::VULKAN, Backend::CPU))

/**
 * @brief Define a performance test with statistical analysis
 */
#define KERNTOPIA_PERFORMANCE_TEST(test_class, test_name, iterations) \
    TEST_F(test_class, test_name) { \
        if (!IsBackendAvailable(config_.target_backend)) { \
            GTEST_SKIP() << "Backend " << config_.GetBackendName() << " not available"; \
        } \
        config_.mode = TestMode::PERFORMANCE; \
        auto stats_result = RunPerformanceTest(iterations); \
        ASSERT_TRUE(stats_result.HasValue()) << "Performance test failed: " << stats_result.GetError().message; \
        auto& stats = *stats_result; \
        EXPECT_GT(stats.sample_count, 0) << "No valid samples collected"; \
        EXPECT_LT(stats.coefficient_of_variation, 0.15f) << "Performance too inconsistent (CV > 15%)"; \
    }

} // namespace kerntopia