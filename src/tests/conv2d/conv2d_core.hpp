#pragma once

#include "core/backend/ikernel_runner.hpp"
#include "core/backend/backend_factory.hpp"
#include "core/common/error_handling.hpp"
#include "core/common/test_params.hpp"
#include <vector>
#include <string>
#include <memory>

namespace kerntopia::conv2d {

class Conv2dCore {
public:
    Conv2dCore(const kerntopia::TestConfiguration& config);
    ~Conv2dCore();

    // Main pipeline functions - Conv2DCore handles kernel loading based on config
    kerntopia::Result<void> Setup(const std::string& input_image_path);
    kerntopia::Result<void> Execute();
    kerntopia::Result<void> WriteOut(const std::string& output_path);
    void TearDown();

    // Expose backend information for testing
    std::string GetDeviceName() const { return kernel_runner_ ? kernel_runner_->GetDeviceName() : "Unknown"; }
    kerntopia::TimingResults GetLastExecutionTime() const { 
        if (kernel_runner_) {
            return kernel_runner_->GetLastExecutionTime();
        } else {
            kerntopia::TimingResults empty_timing{};
            return empty_timing;
        }
    }

private:
    // Configuration and backend abstraction
    kerntopia::TestConfiguration config_;
    std::unique_ptr<kerntopia::IKernelRunner> kernel_runner_;
    
    // Helper functions
    kerntopia::Result<void> LoadKernel();
    std::string GetKernelPath() const;
    
    // Device memory buffers
    std::shared_ptr<kerntopia::IBuffer> d_input_image_;
    std::shared_ptr<kerntopia::IBuffer> d_output_image_;
    std::shared_ptr<kerntopia::IBuffer> d_constants_;
    
    // Image data - using float4 (RGBA) to match SLANG float3 alignment
    std::vector<float> h_input_image_;   // Host input image (RGBA float)
    std::vector<float> h_output_image_;  // Host output image (RGBA float)
    
    // Image dimensions
    uint32_t image_width_;
    uint32_t image_height_;
    
    // Constants buffer structure (matches SLANG cbuffer)
    struct Constants {
        float filter_kernel[4][4];  // 4x4 matrix with 3x3 filter packed in first 3x3 (row-major)
        uint32_t image_width;
        uint32_t image_height;
    };
    
    Constants constants_;
    
    // Helper functions
    kerntopia::Result<void> LoadInputImage(const std::string& input_path);
    kerntopia::Result<void> AllocateDeviceMemory();
    void SetupGaussianFilter();
    kerntopia::Result<void> CopyToDevice();
    kerntopia::Result<void> CopyFromDevice();
};

} // namespace kerntopia::conv2d