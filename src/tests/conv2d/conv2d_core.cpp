#include "conv2d_core.hpp"
#include "core/backend/cuda_runner.hpp"
#include "core/backend/vulkan_runner.hpp"
#include "core/common/logger.hpp"
#include "core/common/path_utils.hpp"
#include <iostream>
#include <fstream>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "../../../third-party/stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION  
#include "../../../third-party/stb/stb_image_write.h"

using namespace kerntopia;

namespace kerntopia::conv2d {

Conv2dCore::Conv2dCore(const TestConfiguration& config) 
    : config_(config)
    , kernel_runner_(nullptr)
    , image_width_(0)
    , image_height_(0) {
}

Conv2dCore::~Conv2dCore() {
    TearDown();
}

Result<void> Conv2dCore::Setup(const std::string& input_image_path) {
    KERNTOPIA_LOG_INFO(LogComponent::TEST, "Setting up Conv2D...");
    
    // Create backend kernel runner based on configuration
    auto backend_result = BackendFactory::CreateRunner(config_.target_backend, config_.device_id);
    if (!backend_result) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Failed to create kernel runner: " + backend_result.GetError().message);
    }
    kernel_runner_ = std::move(backend_result.GetValue());
    
    KERNTOPIA_LOG_DEBUG(LogComponent::TEST, "Created " + config_.GetBackendName() + " kernel runner");
    
    // Load kernel based on configuration
    auto kernel_result = LoadKernel();
    if (!kernel_result) {
        return kernel_result;
    }
    
    auto result = LoadInputImage(input_image_path);
    if (!result) {
        return result;
    }
    
    SetupGaussianFilter();
    
    result = AllocateDeviceMemory();
    if (!result) {
        return result;
    }
    
    result = CopyToDevice();
    if (!result) {
        return result;
    }
    
    KERNTOPIA_LOG_INFO(LogComponent::TEST, "Conv2D setup complete!");
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> Conv2dCore::Execute() {
    KERNTOPIA_LOG_INFO(LogComponent::TEST, "Executing Conv2D kernel...");
    
    // For SLANG-compiled kernels, we need to use backend-specific parameter binding
    // This works for both CUDA (constant memory) and Vulkan (descriptor sets)
    
    Result<void> result;
    if (config_.target_backend == Backend::CUDA) {
        // CUDA-specific buffer pointer binding
        auto cuda_input = std::dynamic_pointer_cast<CudaBuffer>(d_input_image_);
        auto cuda_output = std::dynamic_pointer_cast<CudaBuffer>(d_output_image_);
        auto cuda_constants = std::dynamic_pointer_cast<CudaBuffer>(d_constants_);
        
        if (!cuda_input || !cuda_output || !cuda_constants) {
            return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                         "Failed to cast buffers to CUDA buffers");
        }
        
        // Populate the 40-byte parameter buffer based on PTX analysis:
        // Offset 0: input buffer pointer (8 bytes)
        // Offset 16: output buffer pointer (8 bytes)  
        // Offset 32: constants buffer pointer (8 bytes)
        uint64_t params_buffer[5] = {0}; // 40 bytes total
        params_buffer[0] = reinterpret_cast<uint64_t>(cuda_input->GetDevicePointer());     // Offset 0
        params_buffer[2] = reinterpret_cast<uint64_t>(cuda_output->GetDevicePointer());    // Offset 16 (index 2 = 16/8)
        params_buffer[4] = reinterpret_cast<uint64_t>(cuda_constants->GetDevicePointer()); // Offset 32 (index 4 = 32/8)
        
        KERNTOPIA_LOG_DEBUG(LogComponent::TEST, "CUDA buffer pointers: input=0x" + 
                           std::to_string(params_buffer[0]) + ", output=0x" + 
                           std::to_string(params_buffer[2]) + ", constants=0x" + 
                           std::to_string(params_buffer[4]));
        
        result = kernel_runner_->SetSlangGlobalParameters(params_buffer, sizeof(params_buffer));
    } else if (config_.target_backend == Backend::VULKAN) {
        // Vulkan-specific descriptor set binding
        // For Vulkan, we bind buffers via descriptor sets and pass minimal parameters
        result = kernel_runner_->SetBuffer(0, d_input_image_);   // Binding 0: input image
        if (result) {
            result = kernel_runner_->SetBuffer(1, d_output_image_);  // Binding 1: output image
        }
        if (result) {
            result = kernel_runner_->SetBuffer(2, d_constants_);     // Binding 2: constants
        }
        
        // For Vulkan, SLANG global parameters might include image dimensions and other constants
        struct VulkanParams {
            uint32_t image_width;
            uint32_t image_height;
            uint32_t padding[2]; // Align to 16 bytes
        } vulkan_params = {
            image_width_,
            image_height_,
            {0, 0}
        };
        
        if (result) {
            result = kernel_runner_->SetSlangGlobalParameters(&vulkan_params, sizeof(vulkan_params));
        }
        
        KERNTOPIA_LOG_DEBUG(LogComponent::TEST, "Vulkan buffers bound and parameters set: " + 
                           std::to_string(image_width_) + "x" + std::to_string(image_height_));
    } else {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Unsupported backend for SLANG parameter binding");
    }
    if (!result) {
        return result;
    }
    
    // Calculate grid dimensions (16x16 threads per block)
    uint32_t grid_x, grid_y, grid_z;
    kernel_runner_->CalculateDispatchSize(image_width_, image_height_, 1, grid_x, grid_y, grid_z);
    
    KERNTOPIA_LOG_DEBUG(LogComponent::TEST, "Launching kernel: grid(" + 
                       std::to_string(grid_x) + "x" + std::to_string(grid_y) + "), block(16x16)");
    
    // Launch kernel
    result = kernel_runner_->Dispatch(grid_x, grid_y, grid_z);
    if (!result) {
        return result;
    }
    
    // Wait for completion
    result = kernel_runner_->WaitForCompletion();
    if (!result) {
        return result;
    }
    
    result = CopyFromDevice();
    if (!result) {
        return result;
    }
    
    KERNTOPIA_LOG_INFO(LogComponent::TEST, "Kernel execution complete!");
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> Conv2dCore::WriteOut(const std::string& output_path) {
    KERNTOPIA_LOG_INFO(LogComponent::TEST, "Writing output to: " + output_path);
    
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
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::IMAGING, ErrorCode::IMAGE_SAVE_FAILED,
                                     "Failed to write output image: " + output_path);
    }
    
    KERNTOPIA_LOG_INFO(LogComponent::TEST, "Output written successfully!");
    return KERNTOPIA_VOID_SUCCESS();
}

void Conv2dCore::TearDown() {
    // Clean up owned kernel runner
    kernel_runner_.reset();
    
    // Clear device memory buffers
    d_constants_.reset();
    d_output_image_.reset();
    d_input_image_.reset();
    
    // Clear host memory
    h_input_image_.clear();
    h_output_image_.clear();
}


Result<void> Conv2dCore::LoadInputImage(const std::string& input_path) {
    int width, height, channels;
    uint8_t* image_data = stbi_load(input_path.c_str(), &width, &height, &channels, 3);
    
    if (!image_data) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::IMAGING, ErrorCode::IMAGE_LOAD_FAILED,
                                     "Failed to load image: " + input_path);
    }
    
    image_width_ = static_cast<uint32_t>(width);
    image_height_ = static_cast<uint32_t>(height);
    
    KERNTOPIA_LOG_DEBUG(LogComponent::TEST, "Loaded image: " + std::to_string(width) + "x" + 
                       std::to_string(height) + " (" + std::to_string(channels) + " channels)");
    
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
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> Conv2dCore::AllocateDeviceMemory() {
    size_t image_size = image_width_ * image_height_ * 4 * sizeof(float); // RGBA
    
    KERNTOPIA_LOG_DEBUG(LogComponent::TEST, "Allocating device memory: " + 
                       std::to_string(image_size) + " bytes per image");
    
    // Create input image buffer
    auto input_result = kernel_runner_->CreateBuffer(image_size, IBuffer::Type::STORAGE, IBuffer::Usage::DYNAMIC);
    if (!input_result) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::MEMORY_ALLOCATION_FAILED,
                                     "Failed to allocate input image buffer: " + input_result.GetError().message);
    }
    d_input_image_ = input_result.GetValue();
    
    // Create output image buffer
    auto output_result = kernel_runner_->CreateBuffer(image_size, IBuffer::Type::STORAGE, IBuffer::Usage::DYNAMIC);
    if (!output_result) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::MEMORY_ALLOCATION_FAILED,
                                     "Failed to allocate output image buffer: " + output_result.GetError().message);
    }
    d_output_image_ = output_result.GetValue();
    
    // Create constants buffer
    auto constants_result = kernel_runner_->CreateBuffer(sizeof(Constants), IBuffer::Type::UNIFORM, IBuffer::Usage::STATIC);
    if (!constants_result) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::MEMORY_ALLOCATION_FAILED,
                                     "Failed to allocate constants buffer: " + constants_result.GetError().message);
    }
    d_constants_ = constants_result.GetValue();
    
    return KERNTOPIA_VOID_SUCCESS();
}

void Conv2dCore::SetupGaussianFilter() {
    // 3x3 Gaussian blur filter (approximation of 5x5)
    constants_.image_width = image_width_;
    constants_.image_height = image_height_;
    
    // Normalized 3x3 Gaussian kernel
    constants_.filter_kernel[0][0] = 1.0f/16.0f; constants_.filter_kernel[0][1] = 2.0f/16.0f; constants_.filter_kernel[0][2] = 1.0f/16.0f;
    constants_.filter_kernel[1][0] = 2.0f/16.0f; constants_.filter_kernel[1][1] = 4.0f/16.0f; constants_.filter_kernel[1][2] = 2.0f/16.0f;
    constants_.filter_kernel[2][0] = 1.0f/16.0f; constants_.filter_kernel[2][1] = 2.0f/16.0f; constants_.filter_kernel[2][2] = 1.0f/16.0f;
    
    KERNTOPIA_LOG_DEBUG(LogComponent::TEST, "Gaussian filter setup complete");
}

Result<void> Conv2dCore::CopyToDevice() {
    size_t image_size = image_width_ * image_height_ * 4 * sizeof(float); // RGBA
    
    KERNTOPIA_LOG_DEBUG(LogComponent::TEST, "Copying " + std::to_string(image_size) + " bytes to device...");
    
    // Upload input image
    auto result = d_input_image_->UploadData(h_input_image_.data(), image_size);
    if (!result) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "Failed to copy input image to device: " + result.GetError().message);
    }
    
    // Upload constants
    result = d_constants_->UploadData(&constants_, sizeof(Constants));
    if (!result) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "Failed to copy constants to device: " + result.GetError().message);
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> Conv2dCore::CopyFromDevice() {
    size_t image_size = image_width_ * image_height_ * 4 * sizeof(float); // RGBA
    
    KERNTOPIA_LOG_DEBUG(LogComponent::TEST, "Copying " + std::to_string(image_size) + " bytes from device...");
    
    auto result = d_output_image_->DownloadData(h_output_image_.data(), image_size);
    if (!result) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "Failed to copy output image from device: " + result.GetError().message);
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> Conv2dCore::LoadKernel() {
    KERNTOPIA_LOG_INFO(LogComponent::TEST, "Loading Conv2D kernel...");
    
    std::string kernel_path = GetKernelPath();
    KERNTOPIA_LOG_DEBUG(LogComponent::TEST, "Attempting to load kernel from: " + kernel_path);
    
    // Check if file exists before attempting to open
    std::ifstream existence_check(kernel_path);
    if (!existence_check.good()) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::GENERAL, ErrorCode::FILE_NOT_FOUND,
                                     "Kernel file does not exist at path: " + kernel_path + 
                                     " (working directory relative path)");
    }
    existence_check.close();
    
    std::ifstream file(kernel_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::GENERAL, ErrorCode::FILE_NOT_FOUND,
                                     "Cannot open kernel file (file exists but failed to open): " + kernel_path);
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> bytecode(size);
    if (!file.read(reinterpret_cast<char*>(bytecode.data()), size)) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::IMAGING, ErrorCode::IMAGE_LOAD_FAILED,
                                     "Failed to read kernel file: " + kernel_path);
    }
    
    // Load kernel into the runner
    auto load_result = kernel_runner_->LoadKernel(bytecode, "computeMain");
    if (!load_result) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::KERNEL_LOAD_FAILED,
                                     "Failed to load kernel: " + load_result.GetError().message);
    }
    
    KERNTOPIA_LOG_INFO(LogComponent::TEST, "Kernel loaded successfully from: " + kernel_path);
    return KERNTOPIA_VOID_SUCCESS();
}

std::string Conv2dCore::GetKernelPath() const {
    // Use TestConfiguration to generate the correct kernel filename
    std::string kernel_filename = config_.GetCompiledKernelFilename("conv2d");
    
    // Dynamically determine kernels directory based on executable location
    std::string kernels_dir = PathUtils::GetKernelsDirectory();
    
    return kernels_dir + kernel_filename;
}

} // namespace kerntopia::conv2d