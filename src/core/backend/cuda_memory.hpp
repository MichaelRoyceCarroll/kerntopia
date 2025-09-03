#pragma once

#include "ikernel_runner.hpp"
#include <memory>

// Require CUDA headers - fail compilation if not available
#ifdef KERNTOPIA_CUDA_SDK_AVAILABLE
#include <cuda.h>

// CUDA function pointer types for memory operations
typedef CUresult (*cuMemAlloc_t)(CUdeviceptr* dptr, size_t bytesize);
typedef CUresult (*cuMemFree_t)(CUdeviceptr dptr);
typedef CUresult (*cuMemcpyHtoD_t)(CUdeviceptr dstDevice, const void* srcHost, size_t ByteCount);
typedef CUresult (*cuMemcpyDtoH_t)(void* dstHost, CUdeviceptr srcDevice, size_t ByteCount);
typedef CUresult (*cuGetErrorString_t)(CUresult error, const char** pStr);
#else
#error "CUDA SDK headers are required but not found. Please install CUDA SDK or set CUDA_SDK environment variable."
#endif

namespace kerntopia {

// External CUDA function pointers - initialized by CudaKernelRunner
extern cuMemAlloc_t cu_MemAlloc;
extern cuMemFree_t cu_MemFree;
extern cuMemcpyHtoD_t cu_MemcpyHtoD;
extern cuMemcpyDtoH_t cu_MemcpyDtoH;
extern cuGetErrorString_t cu_GetErrorString;

// Forward declarations
class CudaKernelRunner;

/**
 * @brief CUDA buffer implementation
 */
class CudaBuffer : public IBuffer {
public:
    CudaBuffer(size_t size, Type type, Usage usage);
    ~CudaBuffer();
    
    size_t GetSize() const override { return size_; }
    Type GetType() const override { return type_; }
    
    void* Map() override;
    void Unmap() override;
    
    Result<void> UploadData(const void* data, size_t size, size_t offset = 0) override;
    Result<void> DownloadData(void* data, size_t size, size_t offset = 0) override;
    
    // CUDA-specific methods
    CUdeviceptr GetDevicePointer() const { return device_ptr_; }
    
private:
    size_t size_;
    Type type_;
    Usage usage_;
    CUdeviceptr device_ptr_ = 0;
    void* host_ptr_ = nullptr;
    bool is_mapped_ = false;
};

/**
 * @brief CUDA texture implementation (simplified for compute)
 */
class CudaTexture : public ITexture {
public:
    CudaTexture(const TextureDesc& desc);
    ~CudaTexture();
    
    const TextureDesc& GetDesc() const override { return desc_; }
    
    Result<void> UploadData(const void* data, uint32_t mip_level = 0, uint32_t array_layer = 0) override;
    Result<void> DownloadData(void* data, size_t data_size, uint32_t mip_level = 0, uint32_t array_layer = 0) override;
    
    // CUDA-specific methods
    CUdeviceptr GetDevicePointer() const { return device_ptr_; }
    
private:
    TextureDesc desc_;
    CUdeviceptr device_ptr_ = 0;
    size_t pitch_ = 0;
};

// CUDA error handling utility
std::string CudaErrorToString(CUresult cuda_error);

} // namespace kerntopia