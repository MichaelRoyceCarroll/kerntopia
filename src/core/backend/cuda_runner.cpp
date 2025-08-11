#include "cuda_runner.hpp"
#include "../common/logger.hpp"
#include "runtime_loader.hpp"

#include <sstream>

namespace kerntopia {

// CUDA runtime function pointers (dynamically loaded)
typedef int (*cudaGetDeviceCount_t)(int* count);
typedef int (*cudaGetDeviceProperties_t)(void* prop, int device);
typedef int (*cudaSetDevice_t)(int device);
typedef int (*cudaGetDevice_t)(int* device);
typedef int (*cudaDeviceReset_t)(void);
typedef const char* (*cudaGetErrorString_t)(int error);

// CUDA constants and structures
constexpr int cudaSuccess = 0;
constexpr int cudaErrorInsufficientDriver = 35;
constexpr int cudaErrorNoDevice = 38;

struct cudaDeviceProp {
    char name[256];
    size_t totalGlobalMem;
    size_t sharedMemPerBlock;
    int regsPerBlock;
    int warpSize;
    size_t memPitch;
    int maxThreadsPerBlock;
    int maxThreadsDim[3];
    int maxGridSize[3];
    int clockRate;
    size_t totalConstMem;
    int major;
    int minor;
    size_t textureAlignment;
    size_t texturePitchAlignment;
    int deviceOverlap;
    int multiProcessorCount;
    int kernelExecTimeoutEnabled;
    int integrated;
    int canMapHostMemory;
    int computeMode;
    int maxTexture1D;
    int maxTexture1DMipmap;
    int maxTexture1DLinear;
    int maxTexture2D[2];
    int maxTexture2DMipmap[2];
    int maxTexture2DLinear[3];
    int maxTexture2DGather[2];
    int maxTexture3D[3];
    int maxTexture3DAlt[3];
    int maxTextureCubemap;
    int maxTexture1DLayered[2];
    int maxTexture2DLayered[3];
    int maxTextureCubemapLayered[2];
    int maxSurface1D;
    int maxSurface2D[2];
    int maxSurface3D[3];
    int maxSurface1DLayered[2];
    int maxSurface2DLayered[3];
    int maxSurfaceCubemap;
    int maxSurfaceCubemapLayered[2];
    size_t surfaceAlignment;
    int concurrentKernels;
    int ECCEnabled;
    int pciBusID;
    int pciDeviceID;
    int pciDomainID;
    int tccDriver;
    int asyncEngineCount;
    int unifiedAddressing;
    int memoryClockRate;
    int memoryBusWidth;
    int l2CacheSize;
    int maxThreadsPerMultiProcessor;
    int streamPrioritiesSupported;
    int globalL1CacheSupported;
    int localL1CacheSupported;
    size_t sharedMemPerMultiprocessor;
    int regsPerMultiprocessor;
    int managedMemory;
    int isMultiGpuBoard;
    int multiGpuBoardGroupID;
    int hostNativeAtomicSupported;
    int singleToDoublePrecisionPerfRatio;
    int pageableMemoryAccess;
    int concurrentManagedAccess;
    int computePreemptionSupported;
    int canUseHostPointerForRegisteredMem;
    int cooperativeLaunch;
    int cooperativeMultiDeviceLaunch;
    size_t sharedMemPerBlockOptin;
    int pageableMemoryAccessUsesHostPageTables;
    int directManagedMemAccessFromHost;
};

// CudaKernelRunner implementation (placeholder)
DeviceInfo CudaKernelRunner::GetDeviceInfo() const {
    DeviceInfo info;
    info.name = "CUDA Device (placeholder)";
    info.backend_type = Backend::CUDA;
    return info;
}

Result<void> CudaKernelRunner::LoadKernel(const std::vector<uint8_t>& bytecode, const std::string& entry_point) {
    return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE, 
                                 "CUDA backend not implemented yet");
}

Result<void> CudaKernelRunner::SetParameters(const void* params, size_t size) {
    return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE, 
                                 "CUDA backend not implemented yet");
}

Result<void> CudaKernelRunner::SetBuffer(int binding, std::shared_ptr<IBuffer> buffer) {
    return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE, 
                                 "CUDA backend not implemented yet");
}

Result<void> CudaKernelRunner::SetTexture(int binding, std::shared_ptr<ITexture> texture) {
    return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE, 
                                 "CUDA backend not implemented yet");
}

Result<void> CudaKernelRunner::Dispatch(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) {
    return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE, 
                                 "CUDA backend not implemented yet");
}

Result<void> CudaKernelRunner::WaitForCompletion() {
    return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE, 
                                 "CUDA backend not implemented yet");
}

TimingResults CudaKernelRunner::GetLastExecutionTime() {
    return TimingResults{};
}

Result<std::shared_ptr<IBuffer>> CudaKernelRunner::CreateBuffer(size_t size, IBuffer::Type type, IBuffer::Usage usage) {
    return KERNTOPIA_RESULT_ERROR(std::shared_ptr<IBuffer>, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE, 
                                 "CUDA backend not implemented yet");
}

Result<std::shared_ptr<ITexture>> CudaKernelRunner::CreateTexture(const TextureDesc& desc) {
    return KERNTOPIA_RESULT_ERROR(std::shared_ptr<ITexture>, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE, 
                                 "CUDA backend not implemented yet");
}

void CudaKernelRunner::CalculateDispatchSize(uint32_t width, uint32_t height, uint32_t depth,
                                            uint32_t& groups_x, uint32_t& groups_y, uint32_t& groups_z) {
    groups_x = (width + 15) / 16;
    groups_y = (height + 15) / 16;
    groups_z = depth;
}

std::string CudaKernelRunner::GetDebugInfo() const {
    return "CUDA backend (placeholder implementation)";
}

bool CudaKernelRunner::SupportsFeature(const std::string& feature) const {
    return false; // Placeholder
}

// Static CUDA runtime functions (loaded dynamically)
static cudaGetDeviceCount_t cuda_GetDeviceCount = nullptr;
static cudaGetDeviceProperties_t cuda_GetDeviceProperties = nullptr;
static cudaSetDevice_t cuda_SetDevice = nullptr;
static cudaGetDevice_t cuda_GetDevice = nullptr;
static cudaDeviceReset_t cuda_DeviceReset = nullptr;
static cudaGetErrorString_t cuda_GetErrorString = nullptr;
static LibraryHandle cuda_runtime_handle = nullptr;

// Helper function to load CUDA runtime
static Result<void> LoadCudaRuntime() {
    if (cuda_runtime_handle) {
        return KERNTOPIA_VOID_SUCCESS(); // Already loaded
    }
    
    RuntimeLoader loader;
    
    // Try to find CUDA runtime
    std::vector<std::string> cuda_libs = {"cudart", "cudart64"};
    for (const std::string& lib : cuda_libs) {
        auto find_result = loader.FindLibrary(lib);
        if (find_result) {
            auto load_result = loader.LoadLibrary(find_result->full_path);
            if (load_result) {
                cuda_runtime_handle = *load_result;
                KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Loaded CUDA runtime: " + find_result->full_path);
                break;
            }
        }
    }
    
    if (!cuda_runtime_handle) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to load CUDA runtime library");
    }
    
    // Load required function pointers
    cuda_GetDeviceCount = reinterpret_cast<cudaGetDeviceCount_t>(
        loader.GetSymbol(cuda_runtime_handle, "cudaGetDeviceCount"));
    cuda_GetDeviceProperties = reinterpret_cast<cudaGetDeviceProperties_t>(
        loader.GetSymbol(cuda_runtime_handle, "cudaGetDeviceProperties"));
    cuda_SetDevice = reinterpret_cast<cudaSetDevice_t>(
        loader.GetSymbol(cuda_runtime_handle, "cudaSetDevice"));
    cuda_GetDevice = reinterpret_cast<cudaGetDevice_t>(
        loader.GetSymbol(cuda_runtime_handle, "cudaGetDevice"));
    cuda_DeviceReset = reinterpret_cast<cudaDeviceReset_t>(
        loader.GetSymbol(cuda_runtime_handle, "cudaDeviceReset"));
    cuda_GetErrorString = reinterpret_cast<cudaGetErrorString_t>(
        loader.GetSymbol(cuda_runtime_handle, "cudaGetErrorString"));
    
    // Verify all functions were loaded
    if (!cuda_GetDeviceCount || !cuda_GetDeviceProperties || !cuda_SetDevice || 
        !cuda_GetDevice || !cuda_DeviceReset || !cuda_GetErrorString) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to load required CUDA runtime functions");
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

// Helper to convert CUDA error to string
static std::string CudaErrorString(int cuda_error) {
    if (cuda_GetErrorString) {
        const char* error_str = cuda_GetErrorString(cuda_error);
        return error_str ? std::string(error_str) : "Unknown CUDA error";
    }
    return "CUDA error " + std::to_string(cuda_error);
}

// CudaKernelRunnerFactory implementation
bool CudaKernelRunnerFactory::IsAvailable() const {
    auto load_result = LoadCudaRuntime();
    if (!load_result) {
        KERNTOPIA_LOG_WARNING(LogComponent::BACKEND, "CUDA runtime not available: " + load_result.GetError().message);
        return false;
    }
    
    int device_count = 0;
    int cuda_result = cuda_GetDeviceCount(&device_count);
    
    if (cuda_result != cudaSuccess) {
        KERNTOPIA_LOG_WARNING(LogComponent::BACKEND, "CUDA device detection failed: " + CudaErrorString(cuda_result));
        return false;
    }
    
    return device_count > 0;
}

std::vector<DeviceInfo> CudaKernelRunnerFactory::EnumerateDevices() const {
    std::vector<DeviceInfo> devices;
    
    auto load_result = LoadCudaRuntime();
    if (!load_result) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "Cannot enumerate CUDA devices: " + load_result.GetError().message);
        return devices;
    }
    
    int device_count = 0;
    int cuda_result = cuda_GetDeviceCount(&device_count);
    
    if (cuda_result != cudaSuccess) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "cudaGetDeviceCount failed: " + CudaErrorString(cuda_result));
        return devices;
    }
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Found " + std::to_string(device_count) + " CUDA devices");
    
    for (int device_id = 0; device_id < device_count; ++device_id) {
        cudaDeviceProp prop = {};
        cuda_result = cuda_GetDeviceProperties(&prop, device_id);
        
        if (cuda_result != cudaSuccess) {
            KERNTOPIA_LOG_WARNING(LogComponent::BACKEND, 
                "Failed to get properties for CUDA device " + std::to_string(device_id) + ": " + CudaErrorString(cuda_result));
            continue;
        }
        
        DeviceInfo info;
        info.device_id = device_id;
        info.name = std::string(prop.name);
        info.backend_type = Backend::CUDA;
        info.total_memory_bytes = prop.totalGlobalMem;
        info.free_memory_bytes = prop.totalGlobalMem; // Approximation - would need cudaMemGetInfo for exact
        
        // Build compute capability string
        std::ostringstream cc_stream;
        cc_stream << prop.major << "." << prop.minor;
        info.compute_capability = cc_stream.str();
        
        info.max_threads_per_group = static_cast<uint32_t>(prop.maxThreadsPerBlock);
        info.max_shared_memory_bytes = static_cast<uint32_t>(prop.sharedMemPerBlock);
        info.multiprocessor_count = static_cast<uint32_t>(prop.multiProcessorCount);
        info.base_clock_mhz = static_cast<uint32_t>(prop.clockRate / 1000); // CUDA reports in kHz
        info.memory_bandwidth_gbps = static_cast<float>(prop.memoryBusWidth * prop.memoryClockRate * 2) / (8.0f * 1000000.0f); // Rough calculation
        info.is_integrated = (prop.integrated != 0);
        info.supports_compute = true;
        info.supports_graphics = false; // CUDA is compute-only
        
        // Set API version (approximate based on compute capability)
        if (prop.major >= 8) {
            info.api_version = "CUDA 11.0+";
        } else if (prop.major >= 7) {
            info.api_version = "CUDA 10.0+";
        } else if (prop.major >= 6) {
            info.api_version = "CUDA 9.0+";
        } else {
            info.api_version = "CUDA 8.0+";
        }
        
        devices.push_back(info);
        
        KERNTOPIA_LOG_INFO(LogComponent::BACKEND, 
            "CUDA Device " + std::to_string(device_id) + ": " + info.name + 
            " (CC " + info.compute_capability + ", " + 
            std::to_string(info.total_memory_bytes / (1024*1024)) + " MB)");
    }
    
    return devices;
}

Result<std::unique_ptr<IKernelRunner>> CudaKernelRunnerFactory::CreateRunner(int device_id) const {
    auto load_result = LoadCudaRuntime();
    if (!load_result) {
        return KERNTOPIA_RESULT_ERROR(std::unique_ptr<IKernelRunner>, ErrorCategory::BACKEND,
                                     ErrorCode::BACKEND_NOT_AVAILABLE, "CUDA runtime not available");
    }
    
    // Validate device ID
    int device_count = 0;
    int cuda_result = cuda_GetDeviceCount(&device_count);
    if (cuda_result != cudaSuccess || device_id < 0 || device_id >= device_count) {
        return KERNTOPIA_RESULT_ERROR(std::unique_ptr<IKernelRunner>, ErrorCategory::VALIDATION,
                                     ErrorCode::INVALID_ARGUMENT, 
                                     "Invalid CUDA device ID: " + std::to_string(device_id));
    }
    
    // Set device
    cuda_result = cuda_SetDevice(device_id);
    if (cuda_result != cudaSuccess) {
        return KERNTOPIA_RESULT_ERROR(std::unique_ptr<IKernelRunner>, ErrorCategory::BACKEND,
                                     ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Failed to set CUDA device " + std::to_string(device_id) + ": " + CudaErrorString(cuda_result));
    }
    
    // Create and initialize runner
    std::unique_ptr<IKernelRunner> runner = std::make_unique<CudaKernelRunner>();
    // TODO: Initialize runner with device-specific context
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Created CUDA kernel runner for device " + std::to_string(device_id));
    return Result<std::unique_ptr<IKernelRunner>>::Success(std::move(runner));
}

std::string CudaKernelRunnerFactory::GetVersion() const {
    auto load_result = LoadCudaRuntime();
    if (!load_result) {
        return "Not Available";
    }
    
    // TODO: Query actual CUDA runtime version
    return "CUDA Runtime (Dynamic)";
}

} // namespace kerntopia