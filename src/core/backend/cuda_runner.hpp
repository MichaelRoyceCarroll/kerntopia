#pragma once

#include "ikernel_runner.hpp"
#include "cuda_memory.hpp"
#include "../system/system_interrogator.hpp"
#include <memory>
#include <map>
#include <chrono>

// Require CUDA headers - fail compilation if not available
#ifdef KERNTOPIA_CUDA_SDK_AVAILABLE
#include <cuda.h>
#else
#error "CUDA SDK headers are required but not found. Please install CUDA SDK or set CUDA_SDK environment variable."
#endif

namespace kerntopia {

// Forward declarations for CUDA types
struct CudaContext;
struct CudaModule;
struct CudaFunction;
struct CudaEvent;
struct CudaDeviceMemory;

// Memory classes are now defined in cuda_memory.hpp

/**
 * @brief CUDA backend kernel runner implementation
 */
class CudaKernelRunner : public IKernelRunner {
public:
    CudaKernelRunner(int device_id);
    ~CudaKernelRunner();
    
    // IKernelRunner interface implementation
    std::string GetBackendName() const override { return "CUDA"; }
    std::string GetDeviceName() const override;
    DeviceInfo GetDeviceInfo() const override;
    
    Result<void> LoadKernel(const std::vector<uint8_t>& bytecode, const std::string& entry_point) override;
    Result<void> SetParameters(const void* params, size_t size) override;
    Result<void> SetBuffer(int binding, std::shared_ptr<IBuffer> buffer) override;
    Result<void> SetTexture(int binding, std::shared_ptr<ITexture> texture) override;
    Result<void> Dispatch(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) override;
    Result<void> WaitForCompletion() override;
    TimingResults GetLastExecutionTime() override;
    Result<std::shared_ptr<IBuffer>> CreateBuffer(size_t size, IBuffer::Type type, IBuffer::Usage usage) override;
    Result<std::shared_ptr<ITexture>> CreateTexture(const TextureDesc& desc) override;
    void CalculateDispatchSize(uint32_t width, uint32_t height, uint32_t depth,
                              uint32_t& groups_x, uint32_t& groups_y, uint32_t& groups_z) override;
    std::string GetDebugInfo() const override;
    bool SupportsFeature(const std::string& feature) const override;
    Result<void> SetSlangGlobalParameters(const void* params, size_t size) override;
    
    // Static utility methods for error handling
    static std::string CudaErrorToString(CUresult cuda_error);

private:
    int device_id_;
    std::unique_ptr<CudaContext> context_;
    std::unique_ptr<CudaModule> module_;
    std::unique_ptr<CudaFunction> function_;
    
    // Timing events
    std::unique_ptr<CudaEvent> start_event_;
    std::unique_ptr<CudaEvent> stop_event_;
    std::unique_ptr<CudaEvent> memory_start_event_;
    std::unique_ptr<CudaEvent> memory_stop_event_;
    
    // Parameter management
    std::vector<uint8_t> parameter_buffer_;
    std::map<int, CUdeviceptr> buffer_bindings_;  // binding -> device pointer
    
    // Timing results
    TimingResults last_timing_;
    
    // Helper methods
    Result<void> InitializeCudaContext();
    Result<void> CreateTimingEvents();
};

/**
 * @brief CUDA backend factory
 */
class CudaKernelRunnerFactory : public IKernelRunnerFactory {
public:
    bool IsAvailable() const override;
    std::vector<DeviceInfo> EnumerateDevices() const override;
    Result<std::unique_ptr<IKernelRunner>> CreateRunner(int device_id = 0) const override;
    Backend GetBackendType() const override { return Backend::CUDA; }
    std::string GetVersion() const override;

};

} // namespace kerntopia