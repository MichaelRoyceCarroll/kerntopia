#include "tests/common/base_test.hpp"
#include "core/kernel/kernel_manager.hpp"
#include <filesystem>
#include <fstream>
#include <array>

namespace kerntopia {

/**
 * @brief 2D Convolution kernel test implementation
 * 
 * Tests the conv2d.slang kernel across multiple backends with various
 * filter configurations and image sizes. Validates both functional
 * correctness and performance characteristics.
 */
class Conv2DTest : public ImageKernelTest {
protected:
    void SetUp() override {
        ImageKernelTest::SetUp();
        test_name_ = "conv2d";
        
        // Configure test for conv2d kernel
        ConfigureConv2DTest();
    }
    
    /**
     * @brief Configure conv2d-specific test parameters
     */
    void ConfigureConv2DTest() {
        // Set up 3x3 edge detection filter (Sobel-like)
        filter_kernel_ = {
            {-1.0f, -1.0f, -1.0f},
            {-1.0f,  8.0f, -1.0f},
            {-1.0f, -1.0f, -1.0f}
        };
        
        // Configure test parameters
        config_.SetParam("filter_type", std::string("edge_detection"));
        config_.size = TestSize::HD_1080P; // Start with 1080p
        config_.format = ImageFormat::RGB32F; // Use float format for better precision
        config_.validation_tolerance = 1e-3f; // Allow small numerical differences
    }
    
    /**
     * @brief Load input data for conv2d test
     */
    Result<void> LoadInputData() override {
        // Load test image - use a simple pattern for testing if no test image exists
        std::string input_path = GetTestDataPath("conv2d") + "/input.png";
        
        if (!std::filesystem::exists(input_path)) {
            // Generate synthetic test image if no test data exists
            KERNTOPIA_LOG_INFO(LogComponent::TEST, "Generating synthetic test image for conv2d");
            auto result = GenerateSyntheticImage();
            if (!result.HasValue()) {
                return result.GetError();
            }
            input_image_ = *result;
        } else {
            // Load actual test image
            auto result = LoadImage(input_path, config_.format);
            if (!result.HasValue()) {
                return result.GetError();
            }
            input_image_ = *result;
        }
        
        // Try to load reference output if available
        std::string reference_path = GetTestDataPath("conv2d") + "/reference_output.png";
        if (std::filesystem::exists(reference_path)) {
            auto result = LoadImage(reference_path, config_.format);
            if (result.HasValue()) {
                reference_image_ = *result;
                has_reference_ = true;
            }
        }
        
        KERNTOPIA_LOG_INFO(LogComponent::TEST, "Conv2D test data loaded: " + 
                          std::to_string(input_image_.width) + "x" + std::to_string(input_image_.height));
        return KERNTOPIA_VOID_SUCCESS();
    }
    
    /**
     * @brief Execute conv2d kernel with current configuration
     */
    Result<KernelResult> ExecuteKernel() override {
        if (!kernel_runner_) {
            return KERNTOPIA_RESULT_ERROR(KernelResult, ErrorCategory::TEST, ErrorCode::INVALID_STATE,
                                         "Kernel runner not initialized");
        }
        
        // Load compiled kernel
        std::string kernel_path = GetKernelPath("conv2d");
        std::vector<uint8_t> kernel_bytecode;
        
        auto load_result = LoadKernelBytecode(kernel_path, kernel_bytecode);
        if (!load_result.HasValue()) {
            return load_result.GetError();
        }
        
        // Load kernel into runner
        auto kernel_load_result = kernel_runner_->LoadKernel(kernel_bytecode, "computeMain");
        if (!kernel_load_result.HasValue()) {
            return kernel_load_result.GetError();
        }
        
        // Create buffers
        size_t image_size = input_image_.width * input_image_.height * sizeof(float) * 3; // RGB32F
        
        auto input_buffer = kernel_runner_->CreateBuffer(image_size, IBuffer::Type::STORAGE, IBuffer::Usage::STATIC);
        if (!input_buffer.HasValue()) {
            return input_buffer.GetError();
        }
        
        auto output_buffer = kernel_runner_->CreateBuffer(image_size, IBuffer::Type::STORAGE, IBuffer::Usage::STATIC);
        if (!output_buffer.HasValue()) {
            return output_buffer.GetError();
        }
        
        // Upload input data
        auto upload_result = (*input_buffer)->UploadData(input_image_.data.data(), 
                                                         input_image_.data.size() * sizeof(float));
        if (!upload_result.HasValue()) {
            return upload_result.GetError();
        }
        
        // Set up kernel parameters
        Conv2DParams params;
        params.image_width = input_image_.width;
        params.image_height = input_image_.height;
        
        // Copy filter kernel
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                params.filter_kernel[i][j] = filter_kernel_[i][j];
            }
        }
        
        auto params_result = kernel_runner_->SetParameters(&params, sizeof(params));
        if (!params_result.HasValue()) {
            return params_result.GetError();
        }
        
        // Bind buffers
        auto bind_input_result = kernel_runner_->SetBuffer(0, *input_buffer);
        if (!bind_input_result.HasValue()) {
            return bind_input_result.GetError();
        }
        
        auto bind_output_result = kernel_runner_->SetBuffer(1, *output_buffer);
        if (!bind_output_result.HasValue()) {
            return bind_output_result.GetError();
        }
        
        // Calculate dispatch size
        uint32_t groups_x, groups_y, groups_z;
        kernel_runner_->CalculateDispatchSize(input_image_.width, input_image_.height, 1,
                                            groups_x, groups_y, groups_z);
        
        // Dispatch kernel
        auto dispatch_start = std::chrono::high_resolution_clock::now();
        auto dispatch_result = kernel_runner_->Dispatch(groups_x, groups_y, groups_z);
        if (!dispatch_result.HasValue()) {
            return dispatch_result.GetError();
        }
        
        // Wait for completion
        auto wait_result = kernel_runner_->WaitForCompletion();
        if (!wait_result.HasValue()) {
            return wait_result.GetError();
        }
        auto dispatch_end = std::chrono::high_resolution_clock::now();
        
        // Download output data
        output_image_.width = input_image_.width;
        output_image_.height = input_image_.height;
        output_image_.channels = input_image_.channels;
        output_image_.format = input_image_.format;
        output_image_.data.resize(input_image_.data.size());
        
        auto download_result = (*output_buffer)->DownloadData(output_image_.data.data(),
                                                             output_image_.data.size() * sizeof(float));
        if (!download_result.HasValue()) {
            return download_result.GetError();
        }
        
        // Create result
        KernelResult result;
        result.timing_results = kernel_runner_->GetLastExecutionTime();
        result.success = true;
        result.backend_name = kernel_runner_->GetBackendName();
        result.kernel_name = "conv2d";
        
        // Calculate total execution time
        auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(dispatch_end - dispatch_start);
        result.timing_results.total_time_ms = total_duration.count() / 1000.0f;
        
        KERNTOPIA_LOG_INFO(LogComponent::TEST, "Conv2D kernel executed successfully: " +
                          std::to_string(result.timing_results.compute_time_ms) + "ms");
        
        return Result<KernelResult>::Success(result);
    }

private:
    /**
     * @brief Conv2D kernel parameters structure
     */
    struct Conv2DParams {
        uint32_t image_width;
        uint32_t image_height;
        float filter_kernel[3][3];
    };
    
    /**
     * @brief Generate synthetic test image for conv2d testing
     */
    Result<ImageData> GenerateSyntheticImage() {
        uint32_t width, height;
        config_.GetImageDimensions(width, height);
        
        ImageData image;
        image.width = width;
        image.height = height;
        image.channels = 3;
        image.format = ImageFormat::RGB32F;
        image.data.resize(width * height * 3);
        
        // Generate a simple test pattern (checkerboard + gradient)
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                size_t index = (y * width + x) * 3;
                
                // Checkerboard pattern
                bool checker = ((x / 64) + (y / 64)) % 2 == 0;
                float base = checker ? 0.8f : 0.2f;
                
                // Add gradient
                float gradient = static_cast<float>(x) / width;
                
                image.data[index + 0] = base + gradient * 0.3f;  // R
                image.data[index + 1] = base * 0.8f;             // G  
                image.data[index + 2] = base * 0.6f;             // B
            }
        }
        
        KERNTOPIA_LOG_INFO(LogComponent::TEST, "Generated synthetic test image: " + 
                          std::to_string(width) + "x" + std::to_string(height));
        return Result<ImageData>::Success(image);
    }
    
    /**
     * @brief Load kernel bytecode from compiled file
     */
    Result<void> LoadKernelBytecode(const std::string& kernel_path, std::vector<uint8_t>& bytecode) {
        if (!std::filesystem::exists(kernel_path)) {
            return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::TEST, ErrorCode::FILE_NOT_FOUND,
                                         "Compiled kernel not found: " + kernel_path);
        }
        
        std::ifstream file(kernel_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::TEST, ErrorCode::FILE_ACCESS_ERROR,
                                         "Failed to open kernel file: " + kernel_path);
        }
        
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        bytecode.resize(file_size);
        file.read(reinterpret_cast<char*>(bytecode.data()), file_size);
        
        if (file.fail()) {
            return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::TEST, ErrorCode::FILE_ACCESS_ERROR,
                                         "Failed to read kernel file: " + kernel_path);
        }
        
        KERNTOPIA_LOG_DEBUG(LogComponent::TEST, "Loaded kernel bytecode: " + 
                            std::to_string(bytecode.size()) + " bytes from " + kernel_path);
        return KERNTOPIA_VOID_SUCCESS();
    }

private:
    // Conv2D-specific test data
    std::array<std::array<float, 3>, 3> filter_kernel_;
};

// Parameterized test across all available backends
class Conv2DParameterizedTest : public Conv2DTest, 
                               public ::testing::WithParamInterface<Backend> {
protected:
    void SetUp() override {
        config_.target_backend = GetParam();
        Conv2DTest::SetUp();
    }
};

// Functional tests
TEST_P(Conv2DParameterizedTest, FunctionalTest) {
    if (!IsBackendAvailable(GetParam())) {
        GTEST_SKIP() << "Backend " << config_.GetBackendName() << " not available";
    }
    
    auto result = RunFunctionalTest();
    ASSERT_TRUE(result.HasValue()) << "Conv2D test execution failed: " << result.GetError().message;
    EXPECT_TRUE(result->IsValid()) << "Conv2D kernel validation failed";
}

TEST_P(Conv2DParameterizedTest, EdgeDetectionFilter) {
    if (!IsBackendAvailable(GetParam())) {
        GTEST_SKIP() << "Backend " << config_.GetBackendName() << " not available";
    }
    
    // Test with edge detection filter
    config_.SetParam("filter_type", std::string("edge_detection"));
    
    auto result = RunFunctionalTest();
    ASSERT_TRUE(result.HasValue()) << "Edge detection test failed: " << result.GetError().message;
    EXPECT_TRUE(result->IsValid()) << "Edge detection validation failed";
}

// Performance tests
KERNTOPIA_PERFORMANCE_TEST(Conv2DTest, Performance1080p, 10);

TEST_F(Conv2DTest, Performance4K) {
    if (!IsBackendAvailable(config_.target_backend)) {
        GTEST_SKIP() << "Backend " << config_.GetBackendName() << " not available";
    }
    
    config_.mode = TestMode::PERFORMANCE;
    config_.size = TestSize::UHD_4K;
    
    auto stats_result = RunPerformanceTest(5); // Fewer iterations for 4K
    ASSERT_TRUE(stats_result.HasValue()) << "4K performance test failed: " << stats_result.GetError().message;
    
    auto& stats = *stats_result;
    EXPECT_GT(stats.sample_count, 0) << "No valid samples collected for 4K test";
    EXPECT_LT(stats.coefficient_of_variation, 0.20f) << "4K performance too inconsistent (CV > 20%)";
}

// Instantiate parameterized tests
INSTANTIATE_TEST_SUITE_P(
    AllBackends,
    Conv2DParameterizedTest,
    ::testing::Values(Backend::CUDA, Backend::VULKAN, Backend::CPU),
    [](const ::testing::TestParamInfo<Backend>& info) {
        switch (info.param) {
            case Backend::CUDA: return "CUDA";
            case Backend::VULKAN: return "Vulkan";
            case Backend::CPU: return "CPU";
            default: return "Unknown";
        }
    }
);

} // namespace kerntopia