#include "tests/common/base_test.hpp"
#include "conv2d_core.hpp"
#include <gtest/gtest.h>
#include <fstream>

namespace kerntopia {

/**
 * @brief Conv2D functional test implementation using IKernelRunner abstraction
 */
class Conv2DFunctionalTest : public BaseKernelTest {
protected:
    void SetUp() override {
        BaseKernelTest::SetUp();
        
        // Set up test configuration
        config_.target_backend = Backend::CUDA;  // Default to CUDA for testing
        config_.slang_profile = SlangProfile::CUDA_SM_7_0;
        config_.slang_target = SlangTarget::PTX;
        config_.compilation_mode = CompilationMode::PRECOMPILED;
        config_.mode = TestMode::FUNCTIONAL;
        config_.size = TestSize::CUSTOM;
        config_.custom_width = 512;
        config_.custom_height = 512;
        
        // Use the provided test image
        test_input_image_path_ = "/home/mcarr/kerntopia/assets/images/StockSnap_2Q79J32WX2_512x512.png";
    }
    
    // Implement BaseKernelTest pure virtual function
    Result<KernelResult> ExecuteKernel() override {
        // Create Conv2DCore with configuration - it will handle kernel loading internally
        conv2d::Conv2dCore conv2d_core(config_, kernel_runner_.get());
        
        // Setup with image path - Conv2DCore handles kernel loading based on config
        auto setup_result = conv2d_core.Setup(test_input_image_path_);
        if (!setup_result) {
            return KERNTOPIA_RESULT_ERROR(KernelResult, ErrorCategory::BACKEND, ErrorCode::KERNEL_EXECUTION_FAILED,
                                         "Conv2D setup failed: " + setup_result.GetError().message);
        }
        
        // Execute the kernel
        auto execute_result = conv2d_core.Execute();
        if (!execute_result) {
            return KERNTOPIA_RESULT_ERROR(KernelResult, ErrorCategory::BACKEND, ErrorCode::KERNEL_EXECUTION_FAILED,
                                         "Conv2D execution failed: " + execute_result.GetError().message);
        }
        
        // Write output image for verification
        std::string output_path = "conv2d_output.png";
        auto write_result = conv2d_core.WriteOut(output_path);
        if (!write_result) {
            return KERNTOPIA_RESULT_ERROR(KernelResult, ErrorCategory::IMAGING, ErrorCode::IMAGE_SAVE_FAILED,
                                         "Failed to write output: " + write_result.GetError().message);
        }
        
        // Get timing information
        auto timing = kernel_runner_->GetLastExecutionTime();
        
        // Create successful kernel result
        KernelResult result;
        result.success = true;
        result.kernel_name = "conv2d";
        result.backend_name = config_.GetBackendName();
        result.device_name = kernel_runner_->GetDeviceName();
        result.timing = timing;
        result.AddMetric("output_file_path", 0.0f); // Store as metadata instead
        
        return Result<KernelResult>::Success(result);
    }

private:
    std::string test_input_image_path_;
};

// Parameterized test configuration
using Conv2DTestParams = std::tuple<Backend, SlangProfile, SlangTarget, int>;

// Get valid backend combinations for testing
std::vector<Conv2DTestParams> GetValidCombinations() {
    std::vector<Conv2DTestParams> valid_params;
    
    // CUDA backend with PTX target
    valid_params.push_back(std::make_tuple(Backend::CUDA, SlangProfile::CUDA_SM_7_0, SlangTarget::PTX, 0));
    
    // Vulkan backend with SPIR-V target  
    valid_params.push_back(std::make_tuple(Backend::VULKAN, SlangProfile::GLSL_450, SlangTarget::SPIRV, 0));
    
    // Additional device IDs for systems with multiple GPUs
    valid_params.push_back(std::make_tuple(Backend::CUDA, SlangProfile::CUDA_SM_7_0, SlangTarget::PTX, 1));
    valid_params.push_back(std::make_tuple(Backend::VULKAN, SlangProfile::GLSL_450, SlangTarget::SPIRV, 1));
    
    return valid_params;
}

// Parameterized test across all valid backend combinations
class Conv2DParameterizedTest : public Conv2DFunctionalTest, 
                               public ::testing::WithParamInterface<Conv2DTestParams> {
protected:
    void SetUp() override {
        auto params = GetParam();
        Backend backend = std::get<0>(params);
        SlangProfile profile = std::get<1>(params);
        SlangTarget target = std::get<2>(params);
        int device_id = std::get<3>(params);
        
        // Configure test based on parameters
        config_.target_backend = backend;
        config_.slang_profile = profile;
        config_.slang_target = target;
        config_.device_id = device_id;
        config_.compilation_mode = CompilationMode::PRECOMPILED;
        config_.mode = TestMode::FUNCTIONAL;
        
        Conv2DFunctionalTest::SetUp();
    }
};

// Main functional test - runs across all backend combinations
TEST_P(Conv2DParameterizedTest, GaussianFilter) {
    auto params = GetParam();
    Backend backend = std::get<0>(params);
    
    // Skip if backend not available
    if (!IsBackendAvailable(backend)) {
        GTEST_SKIP() << "Backend " << config_.GetBackendName() << " not available";
    }
    
    auto result = RunFunctionalTest();
    ASSERT_TRUE(result.HasValue()) << "Test execution failed: " << result.GetError().message;
    EXPECT_TRUE(result->IsValid()) << "Kernel validation failed";
}

// Instantiate parameterized tests with proper naming
INSTANTIATE_TEST_SUITE_P(
    Conv2D_AllBackends,
    Conv2DParameterizedTest,
    ::testing::ValuesIn(GetValidCombinations()),
    [](const ::testing::TestParamInfo<Conv2DTestParams>& info) {
        Backend backend = std::get<0>(info.param);
        SlangProfile profile = std::get<1>(info.param);
        SlangTarget target = std::get<2>(info.param);
        int device_id = std::get<3>(info.param);
        
        std::string backend_name;
        switch (backend) {
            case Backend::CUDA: backend_name = "CUDA"; break;
            case Backend::VULKAN: backend_name = "VULKAN"; break;
            case Backend::CPU: backend_name = "CPU"; break;
            default: backend_name = "Unknown"; break;
        }
        
        std::string profile_name;
        switch (profile) {
            case SlangProfile::CUDA_SM_7_0: profile_name = "CUDA_SM_7_0"; break;
            case SlangProfile::GLSL_450: profile_name = "GLSL_450"; break;
            default: profile_name = "Unknown"; break;
        }
        
        return backend_name + "_" + profile_name + "_D" + std::to_string(device_id);
    }
);

} // namespace kerntopia