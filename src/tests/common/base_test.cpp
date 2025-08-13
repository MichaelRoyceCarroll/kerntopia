#include "base_test.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace kerntopia {

void BaseKernelTest::SetUp() {
    // Initialize test metadata
    test_start_time_ = std::chrono::system_clock::now();
    auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    test_name_ = std::string(test_info->test_case_name()) + "." + test_info->name();
    
    KERNTOPIA_LOG_INFO(LogComponent::TEST, "Starting test: " + test_name_);
    
    // Initialize backend infrastructure
    auto init_result = InitializeBackend();
    ASSERT_TRUE(init_result.HasValue()) << "Backend factory initialization failed: " << init_result.GetError().message;
    
    // Initialize image loader
    image_loader_ = std::make_unique<ImageLoader>();
    
    // Set up default configuration
    config_ = TestConfiguration{};
    ConfigureTest(); // Allow derived classes to customize
    
    // Set up test directories
    test_data_dir_ = "test_data";
    output_dir_ = config_.temp_dir + "/" + test_name_;
    auto dir_result = CreateOutputDirectory(output_dir_);
    ASSERT_TRUE(dir_result.HasValue()) << "Failed to create output directory: " << dir_result.GetError().message;
    
    // Load input data
    auto load_result = LoadInputData();
    ASSERT_TRUE(load_result.HasValue()) << "Failed to load input data: " << load_result.GetError().message;
    
    KERNTOPIA_LOG_DEBUG(LogComponent::TEST, "Test setup completed for: " + test_name_);
}

void BaseKernelTest::TearDown() {
    // Clean up kernel runner
    if (kernel_runner_) {
        kernel_runner_.reset();
    }
    
    // Clean up backend infrastructure
    ShutdownBackend();
    
    // Calculate test duration
    auto end_time = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - test_start_time_);
    
    KERNTOPIA_LOG_INFO(LogComponent::TEST, "Test completed: " + test_name_ + 
                      " (Duration: " + std::to_string(duration.count()) + "ms)");
}

ValidationResults BaseKernelTest::ValidateOutput(const KernelResult& result) {
    ValidationResults validation;
    validation.passed = result.success;
    validation.validation_method = "basic_success_check";
    
    if (!result.success) {
        validation.passed = false;
        KERNTOPIA_LOG_WARNING(LogComponent::TEST, "Kernel execution failed: " + result.error.message);
    }
    
    return validation;
}

Result<KernelResult> BaseKernelTest::RunFunctionalTest() {
    KERNTOPIA_LOG_DEBUG(LogComponent::TEST, "Running functional test with backend: " + config_.GetBackendName());
    
    // Execute kernel
    auto exec_result = ExecuteKernel();
    if (!exec_result.HasValue()) {
        return exec_result;
    }
    
    KernelResult& result = *exec_result;
    
    // Validate output if requested
    if (config_.validate_output) {
        result.validation = ValidateOutput(result);
        if (!result.validation.passed) {
            KERNTOPIA_LOG_ERROR(LogComponent::TEST, "Output validation failed");
        }
    }
    
    // Save output if requested
    if (config_.save_output && !config_.output_path.empty()) {
        // Implementation depends on kernel type - override in derived classes
    }
    
    KERNTOPIA_LOG_INFO(LogComponent::TEST, std::string("Functional test completed - Success: ") + 
                      (result.success ? "true" : "false"));
    
    return Result<KernelResult>::Success(result);
}

Result<StatisticalSummary> BaseKernelTest::RunPerformanceTest(int iterations) {
    KERNTOPIA_LOG_INFO(LogComponent::TEST, "Running performance test with " + 
                      std::to_string(iterations) + " iterations");
    
    std::vector<KernelResult> results;
    results.reserve(iterations);
    
    // Run multiple iterations
    for (int i = 0; i < iterations; ++i) {
        KERNTOPIA_LOG_DEBUG(LogComponent::TEST, "Performance iteration " + std::to_string(i + 1) + 
                           "/" + std::to_string(iterations));
        
        auto exec_result = ExecuteKernel();
        if (!exec_result.HasValue()) {
            return KERNTOPIA_RESULT_ERROR(StatisticalSummary, ErrorCategory::TEST,
                                        ErrorCode::TEST_EXECUTION_FAILED,
                                        "Performance test failed at iteration " + std::to_string(i + 1) + 
                                        ": " + exec_result.GetError().message);
        }
        
        results.push_back(*exec_result);
        
        // Validate only first and last iterations to save time
        if (config_.validate_output && (i == 0 || i == iterations - 1)) {
            results.back().validation = ValidateOutput(results.back());
        }
    }
    
    // Calculate statistics
    auto stats = CalculateStatistics(results);
    
    KERNTOPIA_LOG_INFO(LogComponent::TEST, "Performance test completed - Mean: " + 
                      std::to_string(stats.mean_time_ms) + "ms, StdDev: " + 
                      std::to_string(stats.std_deviation_ms) + "ms, CV: " + 
                      std::to_string(stats.coefficient_of_variation * 100.0f) + "%");
    
    return Result<StatisticalSummary>::Success(stats);
}

Result<ImageData> BaseKernelTest::LoadImage(const std::string& path, ImageFormat format) {
    if (!image_loader_) {
        return KERNTOPIA_RESULT_ERROR(ImageData, ErrorCategory::TEST,
                                    ErrorCode::TEST_SETUP_FAILED,
                                    "Image loader not initialized");
    }
    
    return image_loader_->LoadImage(path, format);
}

Result<void> BaseKernelTest::SaveImage(const ImageData& image, const std::string& path) {
    if (!image_loader_) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::TEST,
                                    ErrorCode::TEST_SETUP_FAILED,
                                    "Image loader not initialized");
    }
    
    return image_loader_->SaveImage(image, path);
}

ValidationResults BaseKernelTest::CompareImages(const ImageData& expected, const ImageData& actual, float tolerance) {
    ValidationResults validation;
    validation.tolerance = tolerance;
    validation.validation_method = "pixel_difference";
    
    // Check dimensions
    if (expected.width != actual.width || expected.height != actual.height || 
        expected.channels != actual.channels) {
        validation.passed = false;
        return validation;
    }
    
    // Calculate pixel differences
    size_t pixel_count = expected.width * expected.height * expected.channels;
    std::vector<float> differences;
    differences.reserve(pixel_count);
    
    float total_difference = 0.0f;
    float max_difference = 0.0f;
    
    for (size_t i = 0; i < pixel_count; ++i) {
        float expected_val = static_cast<float>(expected.data[i]) / 255.0f;
        float actual_val = static_cast<float>(actual.data[i]) / 255.0f;
        float diff = std::abs(expected_val - actual_val);
        
        differences.push_back(diff);
        total_difference += diff;
        max_difference = std::max(max_difference, diff);
    }
    
    validation.mean_difference = total_difference / pixel_count;
    validation.max_difference = max_difference;
    validation.passed = max_difference <= tolerance;
    
    // Calculate PSNR
    if (validation.mean_difference > 0.0f) {
        float mse = 0.0f;
        for (float diff : differences) {
            mse += diff * diff;
        }
        mse /= pixel_count;
        validation.psnr_db = 20.0f * std::log10(1.0f / std::sqrt(mse));
    } else {
        validation.psnr_db = std::numeric_limits<float>::infinity();
    }
    
    return validation;
}

StatisticalSummary BaseKernelTest::CalculateStatistics(const std::vector<KernelResult>& results) {
    StatisticalSummary summary;
    
    if (results.empty()) {
        return summary;
    }
    
    summary.sample_count = results.size();
    
    // Extract timing data
    std::vector<float> compute_times;
    std::vector<float> total_times;
    std::vector<bool> validation_passed;
    
    for (const auto& result : results) {
        if (result.success) {
            compute_times.push_back(result.timing.compute_time_ms);
            total_times.push_back(result.timing.total_time_ms);
            validation_passed.push_back(result.validation.passed || result.validation.validation_method.empty());
        }
    }
    
    if (compute_times.empty()) {
        return summary;
    }
    
    // Calculate basic statistics
    auto minmax_compute = std::minmax_element(compute_times.begin(), compute_times.end());
    summary.min_time_ms = *minmax_compute.first;
    summary.max_time_ms = *minmax_compute.second;
    
    summary.mean_time_ms = std::accumulate(compute_times.begin(), compute_times.end(), 0.0f) / compute_times.size();
    
    // Calculate standard deviation
    float variance = 0.0f;
    for (float time : compute_times) {
        float diff = time - summary.mean_time_ms;
        variance += diff * diff;
    }
    variance /= compute_times.size();
    summary.std_deviation_ms = std::sqrt(variance);
    
    // Calculate coefficient of variation
    if (summary.mean_time_ms > 0.0f) {
        summary.coefficient_of_variation = summary.std_deviation_ms / summary.mean_time_ms;
    }
    
    // Calculate median
    std::vector<float> sorted_times = compute_times;
    std::sort(sorted_times.begin(), sorted_times.end());
    size_t mid = sorted_times.size() / 2;
    if (sorted_times.size() % 2 == 0) {
        summary.median_time_ms = (sorted_times[mid - 1] + sorted_times[mid]) / 2.0f;
    } else {
        summary.median_time_ms = sorted_times[mid];
    }
    
    // Calculate validation pass rate
    size_t passed_count = std::count(validation_passed.begin(), validation_passed.end(), true);
    summary.validation_pass_rate = static_cast<float>(passed_count) / validation_passed.size();
    
    return summary;
}

Result<void> BaseKernelTest::CreateOutputDirectory(const std::string& path) {
    try {
        std::filesystem::create_directories(path);
        return KERNTOPIA_VOID_SUCCESS();
    } catch (const std::filesystem::filesystem_error& e) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::SYSTEM,
                                    ErrorCode::PERMISSION_DENIED,
                                    "Failed to create directory: " + std::string(e.what()));
    }
}

std::string BaseKernelTest::GenerateUniqueFilename(const std::string& base_name, const std::string& extension) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << base_name << "_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
    ss << "." << extension;
    
    return ss.str();
}

bool BaseKernelTest::IsBackendAvailable(Backend backend) {
    return BackendFactory::IsBackendAvailable(backend);
}

Result<void> BaseKernelTest::InitializeBackend() {
    // BackendFactory uses all static methods, so just initialize the overall system
    auto init_result = BackendFactory::Initialize();
    if (!init_result.HasValue()) {
        return init_result;
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

void BaseKernelTest::ShutdownBackend() {
    BackendFactory::Shutdown();
}

std::string BaseKernelTest::GetTestDataPath(const std::string& kernel_name) {
    return test_data_dir_ + "/" + kernel_name;
}

std::string BaseKernelTest::GetKernelPath(const std::string& kernel_name) {
    std::string filename = config_.GetCompiledKernelFilename(kernel_name);
    return "build/kernels/" + filename;
}

// ParameterizedKernelTest implementation
void ParameterizedKernelTest::SetUp() {
    // Set backend from test parameter
    config_.target_backend = GetTestBackend();
    
    // Call base setup
    BaseKernelTest::SetUp();
}

// ImageKernelTest implementation
void ImageKernelTest::SetUp() {
    BaseKernelTest::SetUp();
    has_reference_ = false;
}

Result<void> ImageKernelTest::LoadTestImages(const std::string& input_name, const std::string& reference_name) {
    std::string data_path = GetTestDataPath("images");
    
    // Load input image
    auto input_result = LoadImage(data_path + "/" + input_name, config_.format);
    if (!input_result.HasValue()) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::TEST,
                                    ErrorCode::FILE_NOT_FOUND,
                                    "Failed to load input image: " + input_result.GetError().message);
    }
    input_image_ = input_result.GetValue();
    
    // Load reference image if provided
    if (!reference_name.empty()) {
        auto ref_result = LoadImage(data_path + "/" + reference_name, config_.format);
        if (ref_result.HasValue()) {
            reference_image_ = ref_result.GetValue();
            has_reference_ = true;
        } else {
            KERNTOPIA_LOG_WARNING(LogComponent::TEST, "Reference image not found: " + reference_name);
        }
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

ValidationResults ImageKernelTest::ValidateOutput(const KernelResult& result) {
    ValidationResults validation = BaseKernelTest::ValidateOutput(result);
    
    if (!validation.passed || !has_reference_) {
        return validation;
    }
    
    // Compare with reference image if available
    validation = CompareImages(reference_image_, output_image_, config_.validation_tolerance);
    
    return validation;
}

} // namespace kerntopia