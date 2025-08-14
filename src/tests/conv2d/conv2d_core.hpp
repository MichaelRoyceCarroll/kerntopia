#pragma once

#include <cuda.h>
#include <vector>
#include <string>

namespace kerntopia::conv2d {

class Conv2dCore {
public:
    Conv2dCore();
    ~Conv2dCore();

    // Main pipeline functions
    bool Setup(const std::string& ptx_path, const std::string& input_image_path);
    bool Execute();
    bool WriteOut(const std::string& output_path);
    void TearDown();

private:
    // CUDA context and module
    CUcontext cuda_context_;
    CUmodule cuda_module_;
    CUfunction conv2d_kernel_;
    
    // Device memory buffers
    CUdeviceptr d_input_image_;
    CUdeviceptr d_output_image_;
    CUdeviceptr d_constants_;
    
    // Image data - using float4 (RGBA) to match SLANG float3 alignment
    std::vector<float> h_input_image_;   // Host input image (RGBA float)
    std::vector<float> h_output_image_;  // Host output image (RGBA float)
    
    // Image dimensions
    uint32_t image_width_;
    uint32_t image_height_;
    
    // Constants buffer structure (matches SLANG cbuffer)
    struct Constants {
        uint32_t image_width;
        uint32_t image_height;
        float filter_kernel[3][3];  // 5x5 Gaussian stored as 3x3 for SLANG compatibility
    };
    
    Constants constants_;
    
    // Helper functions
    bool InitializeCuda();
    bool LoadPTXModule(const std::string& ptx_path);
    bool LoadInputImage(const std::string& input_path);
    bool AllocateDeviceMemory();
    void SetupGaussianFilter();
    bool CopyToDevice();
    bool CopyFromDevice();
};

} // namespace kerntopia::conv2d