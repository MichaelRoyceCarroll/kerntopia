#include "cuda_memory.hpp"
#include "../common/logger.hpp"
#include "../common/error_handling.hpp"

namespace kerntopia {

// CUDA function pointers - defined here and initialized by the CudaKernelRunner
cuMemAlloc_t cu_MemAlloc = nullptr;
cuMemFree_t cu_MemFree = nullptr;
cuMemcpyHtoD_t cu_MemcpyHtoD = nullptr;
cuMemcpyDtoH_t cu_MemcpyDtoH = nullptr;
cuGetErrorString_t cu_GetErrorString = nullptr;

// CudaBuffer implementation
CudaBuffer::CudaBuffer(size_t size, Type type, Usage usage) 
    : size_(size), type_(type), usage_(usage) {
    
    CUresult result = cu_MemAlloc(&device_ptr_, size);
    if (result != CUDA_SUCCESS) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "Failed to allocate CUDA buffer: " + CudaErrorToString(result));
        device_ptr_ = 0;
    }
}

CudaBuffer::~CudaBuffer() {
    if (device_ptr_) {
        cu_MemFree(device_ptr_);
    }
    if (host_ptr_) {
        delete[] static_cast<uint8_t*>(host_ptr_);
    }
}

void* CudaBuffer::Map() {
    if (!is_mapped_ && !host_ptr_) {
        host_ptr_ = new uint8_t[size_];
        is_mapped_ = true;
    }
    return host_ptr_;
}

void CudaBuffer::Unmap() {
    is_mapped_ = false;
    // Keep host_ptr_ allocated for reuse
}

Result<void> CudaBuffer::UploadData(const void* data, size_t size, size_t offset) {
    if (device_ptr_ == 0) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::MEMORY_ALLOCATION_FAILED,
                                     "CUDA buffer not allocated");
    }
    
    if (offset + size > size_) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::VALIDATION, ErrorCode::INVALID_ARGUMENT,
                                     "Upload size exceeds buffer bounds");
    }
    
    CUresult result = cu_MemcpyHtoD(device_ptr_ + offset, data, size);
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "CUDA memory upload failed: " + CudaErrorToString(result));
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> CudaBuffer::DownloadData(void* data, size_t size, size_t offset) {
    if (device_ptr_ == 0) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::MEMORY_ALLOCATION_FAILED,
                                     "CUDA buffer not allocated");
    }
    
    if (offset + size > size_) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::VALIDATION, ErrorCode::INVALID_ARGUMENT,
                                     "Download size exceeds buffer bounds");
    }
    
    CUresult result = cu_MemcpyDtoH(data, device_ptr_ + offset, size);
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "CUDA memory download failed: " + CudaErrorToString(result));
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

// CudaTexture implementation (simplified as buffer for compute)
CudaTexture::CudaTexture(const TextureDesc& desc) : desc_(desc) {
    // For compute shaders, treat texture as linear buffer
    size_t bytes_per_pixel = 4; // Assume RGBA8 for now
    switch (desc.format) {
        case TextureDesc::Format::R8_UNORM: bytes_per_pixel = 1; break;
        case TextureDesc::Format::RG8_UNORM: bytes_per_pixel = 2; break;
        case TextureDesc::Format::RGBA8_UNORM: bytes_per_pixel = 4; break;
        case TextureDesc::Format::R16_FLOAT: bytes_per_pixel = 2; break;
        case TextureDesc::Format::RGBA16_FLOAT: bytes_per_pixel = 8; break;
        case TextureDesc::Format::R32_FLOAT: bytes_per_pixel = 4; break;
        case TextureDesc::Format::RGBA32_FLOAT: bytes_per_pixel = 16; break;
    }
    
    size_t total_size = desc.width * desc.height * desc.depth * bytes_per_pixel;
    
    CUresult result = cu_MemAlloc(&device_ptr_, total_size);
    if (result != CUDA_SUCCESS) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "Failed to allocate CUDA texture: " + CudaErrorToString(result));
        device_ptr_ = 0;
    }
}

CudaTexture::~CudaTexture() {
    if (device_ptr_) {
        cu_MemFree(device_ptr_);
    }
}

Result<void> CudaTexture::UploadData(const void* data, uint32_t mip_level, uint32_t array_layer) {
    if (!device_ptr_) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::MEMORY_ALLOCATION_FAILED,
                                     "CUDA texture not allocated");
    }
    
    // Simple linear upload for compute textures
    size_t bytes_per_pixel = 4; // Simplified
    size_t total_size = desc_.width * desc_.height * desc_.depth * bytes_per_pixel;
    
    CUresult result = cu_MemcpyHtoD(device_ptr_, data, total_size);
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "CUDA texture upload failed: " + CudaErrorToString(result));
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> CudaTexture::DownloadData(void* data, size_t data_size, uint32_t mip_level, uint32_t array_layer) {
    if (!device_ptr_) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::MEMORY_ALLOCATION_FAILED,
                                     "CUDA texture not allocated");
    }
    
    CUresult result = cu_MemcpyDtoH(data, device_ptr_, data_size);
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "CUDA texture download failed: " + CudaErrorToString(result));
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

// CUDA error handling utility
std::string CudaErrorToString(CUresult cuda_error) {
    if (cu_GetErrorString) {
        const char* error_str = nullptr;
        cu_GetErrorString(cuda_error, &error_str);
        return error_str ? std::string(error_str) : "Unknown CUDA error";
    }
    return "CUDA error " + std::to_string(static_cast<int>(cuda_error));
}

} // namespace kerntopia