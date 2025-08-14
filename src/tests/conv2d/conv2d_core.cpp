#include "conv2d_core.hpp"
#include <iostream>
#include <fstream>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "../../../third-party/stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION  
#include "../../../third-party/stb/stb_image_write.h"

namespace kerntopia::conv2d {

Conv2dCore::Conv2dCore() 
    : cuda_context_(nullptr)
    , cuda_module_(nullptr) 
    , conv2d_kernel_(nullptr)
    , d_input_image_(0)
    , d_output_image_(0)
    , d_constants_(0)
    , image_width_(0)
    , image_height_(0) {
}

Conv2dCore::~Conv2dCore() {
    TearDown();
}

bool Conv2dCore::Setup(const std::string& ptx_path, const std::string& input_image_path) {
    std::cout << "Setting up Conv2D CUDA pipeline..." << std::endl;
    
    if (!InitializeCuda()) {
        std::cerr << "Failed to initialize CUDA" << std::endl;
        return false;
    }
    
    if (!LoadPTXModule(ptx_path)) {
        std::cerr << "Failed to load PTX module" << std::endl;
        return false;
    }
    
    if (!LoadInputImage(input_image_path)) {
        std::cerr << "Failed to load input image" << std::endl;
        return false;
    }
    
    SetupGaussianFilter();
    
    if (!AllocateDeviceMemory()) {
        std::cerr << "Failed to allocate device memory" << std::endl;
        return false;
    }
    
    if (!CopyToDevice()) {
        std::cerr << "Failed to copy data to device" << std::endl;
        return false;
    }
    
    std::cout << "Setup complete!" << std::endl;
    return true;
}

bool Conv2dCore::Execute() {
    std::cout << "Executing Conv2D kernel..." << std::endl;
    
    // SLANG-compiled kernels expect buffer pointers in constant memory (SLANG_globalParams)
    // Get the constant memory symbol
    CUdeviceptr slang_params_ptr;
    size_t slang_params_size;
    CUresult result = cuModuleGetGlobal(&slang_params_ptr, &slang_params_size, cuda_module_, "SLANG_globalParams");
    if (result != CUDA_SUCCESS) {
        std::cerr << "Failed to get SLANG_globalParams symbol: " << result << std::endl;
        return false;
    }
    
    std::cout << "SLANG_globalParams size: " << slang_params_size << " bytes" << std::endl;
    
    // Populate the 40-byte parameter buffer based on PTX analysis:
    // Offset 0: input buffer pointer (8 bytes)
    // Offset 16: output buffer pointer (8 bytes)  
    // Offset 32: constants buffer pointer (8 bytes)
    uint64_t params_buffer[5] = {0}; // 40 bytes total
    params_buffer[0] = static_cast<uint64_t>(d_input_image_);   // Offset 0
    params_buffer[2] = static_cast<uint64_t>(d_output_image_);  // Offset 16 (index 2 = 16/8)
    params_buffer[4] = static_cast<uint64_t>(d_constants_);     // Offset 32 (index 4 = 32/8)
    
    std::cout << "Buffer pointers: input=0x" << std::hex << params_buffer[0] 
              << ", output=0x" << params_buffer[2] 
              << ", constants=0x" << params_buffer[4] << std::dec << std::endl;
    
    // Copy buffer pointers to constant memory
    result = cuMemcpyHtoD(slang_params_ptr, params_buffer, std::min(static_cast<size_t>(40), slang_params_size));
    if (result != CUDA_SUCCESS) {
        std::cerr << "Failed to copy parameters to SLANG_globalParams: " << result << std::endl;
        return false;
    }
    
    // Calculate grid dimensions (16x16 threads per block)
    unsigned int grid_x = (image_width_ + 15) / 16;
    unsigned int grid_y = (image_height_ + 15) / 16;
    
    std::cout << "Launching kernel: grid(" << grid_x << "x" << grid_y << "), block(16x16)" << std::endl;
    
    // Launch kernel with no parameters (SLANG kernels get data from constant memory)
    result = cuLaunchKernel(
        conv2d_kernel_,
        grid_x, grid_y, 1,      // Grid dimensions
        16, 16, 1,              // Block dimensions  
        0,                      // Shared memory
        nullptr,                // Stream
        nullptr,                // Parameters (SLANG uses constant memory)
        nullptr                 // Extra
    );
    
    if (result != CUDA_SUCCESS) {
        std::cerr << "Kernel launch failed: " << result << std::endl;
        return false;
    }
    
    // Wait for completion and check for kernel execution errors
    result = cuCtxSynchronize();
    if (result != CUDA_SUCCESS) {
        std::cerr << "Kernel execution failed: " << result << std::endl;
        return false;
    }
    
    if (!CopyFromDevice()) {
        std::cerr << "Failed to copy results from device" << std::endl;
        return false;
    }
    
    std::cout << "Kernel execution complete!" << std::endl;
    return true;
}

bool Conv2dCore::WriteOut(const std::string& output_path) {
    std::cout << "Writing output to: " << output_path << std::endl;
    
    // Convert float RGBA back to uint8 RGB (skip alpha channel)
    std::vector<uint8_t> output_bytes(image_width_ * image_height_ * 3);
    
    for (size_t y = 0; y < image_height_; y++) {
        for (size_t x = 0; x < image_width_; x++) {
            size_t src_idx = (y * image_width_ + x) * 4; // RGBA source
            size_t dst_idx = (y * image_width_ + x) * 3; // RGB destination
            
            // Copy RGB, skip alpha
            for (int c = 0; c < 3; c++) {
                float val = h_output_image_[src_idx + c];
                val = std::max(0.0f, std::min(1.0f, val));
                output_bytes[dst_idx + c] = static_cast<uint8_t>(val * 255.0f);
            }
        }
    }
    
    // Write PNG
    int result = stbi_write_png(
        output_path.c_str(),
        image_width_, image_height_, 3,
        output_bytes.data(),
        image_width_ * 3
    );
    
    if (!result) {
        std::cerr << "Failed to write output image" << std::endl;
        return false;
    }
    
    std::cout << "Output written successfully!" << std::endl;
    return true;
}

void Conv2dCore::TearDown() {
    if (d_constants_) cuMemFree(d_constants_);
    if (d_output_image_) cuMemFree(d_output_image_);
    if (d_input_image_) cuMemFree(d_input_image_);
    if (cuda_module_) cuModuleUnload(cuda_module_);
    if (cuda_context_) cuCtxDestroy(cuda_context_);
    
    d_constants_ = 0;
    d_output_image_ = 0;
    d_input_image_ = 0;
    cuda_module_ = nullptr;
    cuda_context_ = nullptr;
}

bool Conv2dCore::InitializeCuda() {
    CUresult result = cuInit(0);
    if (result != CUDA_SUCCESS) {
        std::cerr << "cuInit failed: " << result << std::endl;
        return false;
    }
    
    CUdevice device;
    result = cuDeviceGet(&device, 0);
    if (result != CUDA_SUCCESS) {
        std::cerr << "cuDeviceGet failed: " << result << std::endl;
        return false;
    }
    
    result = cuCtxCreate(&cuda_context_, 0, device);
    if (result != CUDA_SUCCESS) {
        std::cerr << "cuCtxCreate failed: " << result << std::endl;
        return false;
    }
    
    return true;
}

bool Conv2dCore::LoadPTXModule(const std::string& ptx_path) {
    std::ifstream ptx_file(ptx_path);
    if (!ptx_file.is_open()) {
        std::cerr << "Cannot open PTX file: " << ptx_path << std::endl;
        return false;
    }
    
    std::string ptx_source((std::istreambuf_iterator<char>(ptx_file)),
                          std::istreambuf_iterator<char>());
    ptx_file.close();
    
    CUresult result = cuModuleLoadData(&cuda_module_, ptx_source.c_str());
    if (result != CUDA_SUCCESS) {
        std::cerr << "cuModuleLoadData failed: " << result << std::endl;
        return false;
    }
    
    result = cuModuleGetFunction(&conv2d_kernel_, cuda_module_, "computeMain");
    if (result != CUDA_SUCCESS) {
        std::cerr << "cuModuleGetFunction failed: " << result << std::endl;
        return false;
    }
    
    return true;
}

bool Conv2dCore::LoadInputImage(const std::string& input_path) {
    int width, height, channels;
    uint8_t* image_data = stbi_load(input_path.c_str(), &width, &height, &channels, 3);
    
    if (!image_data) {
        std::cerr << "Failed to load image: " << input_path << std::endl;
        return false;
    }
    
    image_width_ = static_cast<uint32_t>(width);
    image_height_ = static_cast<uint32_t>(height);
    
    std::cout << "Loaded image: " << width << "x" << height << " (" << channels << " channels)" << std::endl;
    
    // Convert uint8 RGB to float RGBA (add alpha = 1.0)
    size_t pixel_count = image_width_ * image_height_;
    h_input_image_.resize(pixel_count * 4);  // RGBA
    h_output_image_.resize(pixel_count * 4); // RGBA
    
    for (size_t i = 0; i < pixel_count; i++) {
        // RGB channels
        h_input_image_[i * 4 + 0] = static_cast<float>(image_data[i * 3 + 0]) / 255.0f; // R
        h_input_image_[i * 4 + 1] = static_cast<float>(image_data[i * 3 + 1]) / 255.0f; // G
        h_input_image_[i * 4 + 2] = static_cast<float>(image_data[i * 3 + 2]) / 255.0f; // B
        h_input_image_[i * 4 + 3] = 1.0f; // Alpha
    }
    
    stbi_image_free(image_data);
    return true;
}

bool Conv2dCore::AllocateDeviceMemory() {
    size_t image_size = image_width_ * image_height_ * 4 * sizeof(float); // RGBA
    
    std::cout << "Allocating device memory: " << image_size << " bytes per image" << std::endl;
    
    CUresult result = cuMemAlloc(&d_input_image_, image_size);
    if (result != CUDA_SUCCESS) {
        std::cerr << "Failed to allocate input image memory: " << result << std::endl;
        return false;
    }
    
    result = cuMemAlloc(&d_output_image_, image_size);
    if (result != CUDA_SUCCESS) {
        std::cerr << "Failed to allocate output image memory: " << result << std::endl;
        return false;
    }
    
    result = cuMemAlloc(&d_constants_, sizeof(Constants));
    if (result != CUDA_SUCCESS) {
        std::cerr << "Failed to allocate constants memory: " << result << std::endl;
        return false;
    }
    
    return true;
}

void Conv2dCore::SetupGaussianFilter() {
    // 3x3 Gaussian blur filter (approximation of 5x5)
    constants_.image_width = image_width_;
    constants_.image_height = image_height_;
    
    // Normalized 3x3 Gaussian kernel
    constants_.filter_kernel[0][0] = 1.0f/16.0f; constants_.filter_kernel[0][1] = 2.0f/16.0f; constants_.filter_kernel[0][2] = 1.0f/16.0f;
    constants_.filter_kernel[1][0] = 2.0f/16.0f; constants_.filter_kernel[1][1] = 4.0f/16.0f; constants_.filter_kernel[1][2] = 2.0f/16.0f;
    constants_.filter_kernel[2][0] = 1.0f/16.0f; constants_.filter_kernel[2][1] = 2.0f/16.0f; constants_.filter_kernel[2][2] = 1.0f/16.0f;
    
    std::cout << "Gaussian filter setup complete" << std::endl;
}

bool Conv2dCore::CopyToDevice() {
    size_t image_size = image_width_ * image_height_ * 4 * sizeof(float); // RGBA
    
    std::cout << "Copying " << image_size << " bytes to device..." << std::endl;
    
    CUresult result = cuMemcpyHtoD(d_input_image_, h_input_image_.data(), image_size);
    if (result != CUDA_SUCCESS) {
        std::cerr << "Failed to copy input image to device: " << result << std::endl;
        return false;
    }
    
    result = cuMemcpyHtoD(d_constants_, &constants_, sizeof(Constants));
    if (result != CUDA_SUCCESS) {
        std::cerr << "Failed to copy constants to device: " << result << std::endl;
        return false;
    }
    
    return true;
}

bool Conv2dCore::CopyFromDevice() {
    size_t image_size = image_width_ * image_height_ * 4 * sizeof(float); // RGBA
    
    std::cout << "Copying " << image_size << " bytes from device..." << std::endl;
    
    CUresult result = cuMemcpyDtoH(h_output_image_.data(), d_output_image_, image_size);
    if (result != CUDA_SUCCESS) {
        std::cerr << "Failed to copy output image from device: " << result << std::endl;
        return false;
    }
    
    return true;
}

} // namespace kerntopia::conv2d