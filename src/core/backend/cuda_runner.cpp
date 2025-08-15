#include "cuda_runner.hpp"
#include "../common/logger.hpp"
#include "runtime_loader.hpp"
#include "../system/system_interrogator.hpp"
#include "../system/interrogation_data.hpp"

#include <sstream>
#include <algorithm>
#include <cstring>

namespace kerntopia {

// CUDA Driver API function pointers (dynamically loaded)
typedef int (*cuInit_t)(unsigned int Flags);
typedef int (*cuDeviceGetCount_t)(int* count);
typedef int (*cuDeviceGet_t)(int* device, int ordinal);
typedef int (*cuDeviceGetName_t)(char* name, int len, int dev);
typedef int (*cuDeviceGetAttribute_t)(int* pi, int attrib, int dev);
typedef int (*cuCtxCreate_t)(void** pctx, unsigned int flags, int dev);
typedef int (*cuCtxDestroy_t)(void* ctx);
typedef int (*cuCtxSetCurrent_t)(void* ctx);
typedef int (*cuModuleLoadData_t)(void** module, const void* image);
typedef int (*cuModuleUnload_t)(void* hmod);
typedef int (*cuModuleGetFunction_t)(void** hfunc, void* hmod, const char* name);
typedef int (*cuModuleGetGlobal_t)(void** dptr, size_t* bytes, void* hmod, const char* name);
typedef int (*cuMemAlloc_t)(void** dptr, size_t bytesize);
typedef int (*cuMemFree_t)(void* dptr);
typedef int (*cuMemcpyHtoD_t)(void* dstDevice, const void* srcHost, size_t ByteCount);
typedef int (*cuMemcpyDtoH_t)(void* dstHost, const void* srcDevice, size_t ByteCount);
typedef int (*cuLaunchKernel_t)(void* f, unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ, 
                               unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ,
                               unsigned int sharedMemBytes, void* hStream, void** kernelParams, void** extra);
typedef int (*cuEventCreate_t)(void** phEvent, unsigned int Flags);
typedef int (*cuEventDestroy_t)(void* hEvent);
typedef int (*cuEventRecord_t)(void* hEvent, void* hStream);
typedef int (*cuEventElapsedTime_t)(float* pMilliseconds, void* hStart, void* hEnd);
typedef int (*cuCtxSynchronize_t)(void);
typedef int (*cuGetErrorString_t)(int error, const char** pStr);

// Static function pointers
static cuInit_t cu_Init = nullptr;
static cuDeviceGetCount_t cu_DeviceGetCount = nullptr;
static cuDeviceGet_t cu_DeviceGet = nullptr;
static cuDeviceGetName_t cu_DeviceGetName = nullptr;
static cuDeviceGetAttribute_t cu_DeviceGetAttribute = nullptr;
static cuCtxCreate_t cu_CtxCreate = nullptr;
static cuCtxDestroy_t cu_CtxDestroy = nullptr;
static cuCtxSetCurrent_t cu_CtxSetCurrent = nullptr;
static cuModuleLoadData_t cu_ModuleLoadData = nullptr;
static cuModuleUnload_t cu_ModuleUnload = nullptr;
static cuModuleGetFunction_t cu_ModuleGetFunction = nullptr;
static cuModuleGetGlobal_t cu_ModuleGetGlobal = nullptr;
static cuMemAlloc_t cu_MemAlloc = nullptr;
static cuMemFree_t cu_MemFree = nullptr;
static cuMemcpyHtoD_t cu_MemcpyHtoD = nullptr;
static cuMemcpyDtoH_t cu_MemcpyDtoH = nullptr;
static cuLaunchKernel_t cu_LaunchKernel = nullptr;
static cuEventCreate_t cu_EventCreate = nullptr;
static cuEventDestroy_t cu_EventDestroy = nullptr;
static cuEventRecord_t cu_EventRecord = nullptr;
static cuEventElapsedTime_t cu_EventElapsedTime = nullptr;
static cuCtxSynchronize_t cu_CtxSynchronize = nullptr;
static cuGetErrorString_t cu_GetErrorString = nullptr;

static LibraryHandle cuda_driver_handle = nullptr;

// CUDA error codes
constexpr int CUDA_SUCCESS = 0;
constexpr int CUDA_ERROR_INVALID_VALUE = 1;
constexpr int CUDA_ERROR_OUT_OF_MEMORY = 2;
constexpr int CUDA_ERROR_NOT_INITIALIZED = 3;
constexpr int CUDA_ERROR_DEINITIALIZED = 4;
constexpr int CUDA_ERROR_NO_DEVICE = 100;
constexpr int CUDA_ERROR_INVALID_DEVICE = 101;

// CUDA device attributes
constexpr int CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK = 1;
constexpr int CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_X = 2;
constexpr int CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Y = 3;
constexpr int CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Z = 4;
constexpr int CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_X = 5;
constexpr int CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Y = 6;
constexpr int CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Z = 7;
constexpr int CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK = 8;
constexpr int CU_DEVICE_ATTRIBUTE_TOTAL_CONSTANT_MEMORY = 9;
constexpr int CU_DEVICE_ATTRIBUTE_WARP_SIZE = 10;
constexpr int CU_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_BLOCK = 12;
constexpr int CU_DEVICE_ATTRIBUTE_CLOCK_RATE = 13;
constexpr int CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT = 16;
constexpr int CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR = 75;
constexpr int CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR = 76;
constexpr int CU_DEVICE_ATTRIBUTE_MEMORY_CLOCK_RATE = 36;
constexpr int CU_DEVICE_ATTRIBUTE_GLOBAL_MEMORY_BUS_WIDTH = 37;

// CUDA context structures
struct CudaContext {
    void* handle = nullptr;
};

struct CudaModule {
    void* handle = nullptr;
};

struct CudaFunction {
    void* handle = nullptr;
};

struct CudaEvent {
    void* handle = nullptr;
};


// Helper function to convert CUDA error to string
std::string CudaKernelRunner::CudaErrorToString(int cuda_error) {
    if (cu_GetErrorString) {
        const char* error_str = nullptr;
        cu_GetErrorString(cuda_error, &error_str);
        return error_str ? std::string(error_str) : "Unknown CUDA error";
    }
    return "CUDA error " + std::to_string(cuda_error);
}

// CudaBuffer implementation
CudaBuffer::CudaBuffer(size_t size, Type type, Usage usage) 
    : size_(size), type_(type), usage_(usage) {
    
    int result = cu_MemAlloc(&device_ptr_, size);
    if (result != CUDA_SUCCESS) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "Failed to allocate CUDA buffer: " + CudaKernelRunner::CudaErrorToString(result));
        device_ptr_ = nullptr;
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
    if (!device_ptr_) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::MEMORY_ALLOCATION_FAILED,
                                     "CUDA buffer not allocated");
    }
    
    if (offset + size > size_) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::VALIDATION, ErrorCode::INVALID_ARGUMENT,
                                     "Upload size exceeds buffer bounds");
    }
    
    int result = cu_MemcpyHtoD(static_cast<uint8_t*>(device_ptr_) + offset, data, size);
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "CUDA memory upload failed: " + CudaKernelRunner::CudaErrorToString(result));
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> CudaBuffer::DownloadData(void* data, size_t size, size_t offset) {
    if (!device_ptr_) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::MEMORY_ALLOCATION_FAILED,
                                     "CUDA buffer not allocated");
    }
    
    if (offset + size > size_) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::VALIDATION, ErrorCode::INVALID_ARGUMENT,
                                     "Download size exceeds buffer bounds");
    }
    
    int result = cu_MemcpyDtoH(data, static_cast<uint8_t*>(device_ptr_) + offset, size);
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "CUDA memory download failed: " + CudaKernelRunner::CudaErrorToString(result));
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
    
    int result = cu_MemAlloc(&device_ptr_, total_size);
    if (result != CUDA_SUCCESS) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "Failed to allocate CUDA texture: " + CudaKernelRunner::CudaErrorToString(result));
        device_ptr_ = nullptr;
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
    
    int result = cu_MemcpyHtoD(device_ptr_, data, total_size);
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "CUDA texture upload failed: " + CudaKernelRunner::CudaErrorToString(result));
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> CudaTexture::DownloadData(void* data, size_t data_size, uint32_t mip_level, uint32_t array_layer) {
    if (!device_ptr_) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::MEMORY_ALLOCATION_FAILED,
                                     "CUDA texture not allocated");
    }
    
    int result = cu_MemcpyDtoH(data, device_ptr_, data_size);
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "CUDA texture download failed: " + CudaKernelRunner::CudaErrorToString(result));
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

// CudaKernelRunner implementation
CudaKernelRunner::CudaKernelRunner(int device_id) : device_id_(device_id) {
    context_ = std::make_unique<CudaContext>();
    module_ = std::make_unique<CudaModule>();
    function_ = std::make_unique<CudaFunction>();
    
    start_event_ = std::make_unique<CudaEvent>();
    stop_event_ = std::make_unique<CudaEvent>();
    memory_start_event_ = std::make_unique<CudaEvent>();
    memory_stop_event_ = std::make_unique<CudaEvent>();
    
    auto init_result = InitializeCudaContext();
    if (!init_result) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "Failed to initialize CUDA context: " + init_result.GetError().message);
    }
    
    auto events_result = CreateTimingEvents();
    if (!events_result) {
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, "Failed to create timing events: " + events_result.GetError().message);
    }
}

CudaKernelRunner::~CudaKernelRunner() {
    // Clean up events
    if (start_event_ && start_event_->handle) cu_EventDestroy(start_event_->handle);
    if (stop_event_ && stop_event_->handle) cu_EventDestroy(stop_event_->handle);
    if (memory_start_event_ && memory_start_event_->handle) cu_EventDestroy(memory_start_event_->handle);
    if (memory_stop_event_ && memory_stop_event_->handle) cu_EventDestroy(memory_stop_event_->handle);
    
    // Clean up module
    if (module_ && module_->handle) cu_ModuleUnload(module_->handle);
    
    // Clean up context
    if (context_ && context_->handle) cu_CtxDestroy(context_->handle);
}

Result<void> CudaKernelRunner::InitializeCudaContext() {
    int device;
    int result = cu_DeviceGet(&device, device_id_);
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Failed to get CUDA device: " + CudaErrorToString(result));
    }
    
    result = cu_CtxCreate(&context_->handle, 0, device);
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "Failed to create CUDA context: " + CudaErrorToString(result));
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> CudaKernelRunner::CreateTimingEvents() {
    int result = cu_EventCreate(&start_event_->handle, 0);
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "Failed to create start event: " + CudaErrorToString(result));
    }
    
    result = cu_EventCreate(&stop_event_->handle, 0);
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "Failed to create stop event: " + CudaErrorToString(result));
    }
    
    result = cu_EventCreate(&memory_start_event_->handle, 0);
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "Failed to create memory start event: " + CudaErrorToString(result));
    }
    
    result = cu_EventCreate(&memory_stop_event_->handle, 0);
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "Failed to create memory stop event: " + CudaErrorToString(result));
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}

std::string CudaKernelRunner::GetDeviceName() const {
    char name[256] = {0};
    cu_DeviceGetName(name, sizeof(name), device_id_);
    return std::string(name);
}

DeviceInfo CudaKernelRunner::GetDeviceInfo() const {
    // CUDA backend handles its own device enumeration with full properties
    DeviceInfo info;
    info.device_id = device_id_;
    info.backend_type = Backend::CUDA;
    
    if (!cuda_driver_handle) {
        info.name = "CUDA Device (driver not loaded)";
        return info;
    }
    
    // Get device name using CUDA Driver API
    char name[256] = {0};
    int result = cu_DeviceGetName(name, sizeof(name), device_id_);
    if (result == CUDA_SUCCESS) {
        info.name = std::string(name);
    } else {
        info.name = "CUDA Device (query failed)";
    }
    
    // Get device attributes using CUDA Driver API
    int value;
    
    // Compute capability
    int major = 0, minor = 0;
    cu_DeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, device_id_);
    cu_DeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, device_id_);
    info.compute_capability = std::to_string(major) + "." + std::to_string(minor);
    
    // Thread and memory limits
    cu_DeviceGetAttribute(&value, CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK, device_id_);
    info.max_threads_per_group = static_cast<uint32_t>(value);
    
    cu_DeviceGetAttribute(&value, CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK, device_id_);
    info.max_shared_memory_bytes = static_cast<uint32_t>(value);
    
    cu_DeviceGetAttribute(&value, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, device_id_);
    info.multiprocessor_count = static_cast<uint32_t>(value);
    
    // Clock rates
    cu_DeviceGetAttribute(&value, CU_DEVICE_ATTRIBUTE_CLOCK_RATE, device_id_);
    info.base_clock_mhz = static_cast<uint32_t>(value / 1000); // Convert from kHz to MHz
    
    cu_DeviceGetAttribute(&value, CU_DEVICE_ATTRIBUTE_MEMORY_CLOCK_RATE, device_id_);
    int memory_clock = value;
    
    cu_DeviceGetAttribute(&value, CU_DEVICE_ATTRIBUTE_GLOBAL_MEMORY_BUS_WIDTH, device_id_);
    int bus_width = value;
    
    // Estimate memory bandwidth: (memory_clock * 2) * (bus_width / 8) / 1e6
    info.memory_bandwidth_gbps = static_cast<float>(memory_clock * 2) * (bus_width / 8) / 1e6f;
    
    // TODO: Query actual memory size using cuMemGetInfo when context is available
    info.total_memory_bytes = 8ULL * 1024 * 1024 * 1024; // Default to 8GB
    info.free_memory_bytes = info.total_memory_bytes;
    
    info.is_integrated = false; // Most discrete CUDA devices
    info.supports_compute = true;
    info.supports_graphics = false;
    
    // API version based on compute capability
    if (major >= 8) {
        info.api_version = "CUDA 11.0+";
    } else if (major >= 7) {
        info.api_version = "CUDA 10.0+";
    } else {
        info.api_version = "CUDA 9.0+";
    }
    
    return info;
}

Result<void> CudaKernelRunner::LoadKernel(const std::vector<uint8_t>& bytecode, const std::string& entry_point) {
    // Ensure context is current
    cu_CtxSetCurrent(context_->handle);
    
    // Load PTX module
    int result = cu_ModuleLoadData(&module_->handle, bytecode.data());
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "Failed to load PTX module: " + CudaErrorToString(result));
    }
    
    // Get kernel function
    result = cu_ModuleGetFunction(&function_->handle, module_->handle, entry_point.c_str());
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "Failed to get kernel function '" + entry_point + "': " + CudaErrorToString(result));
    }
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Loaded CUDA kernel: " + entry_point);
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> CudaKernelRunner::SetParameters(const void* params, size_t size) {
    parameter_buffer_.resize(size);
    std::memcpy(parameter_buffer_.data(), params, size);
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> CudaKernelRunner::SetSlangGlobalParameters(const void* params, size_t size) {
    if (!module_->handle) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "No module loaded");
    }
    
    // Get the SLANG_globalParams constant memory symbol
    void* slang_params_ptr;
    size_t slang_params_size;
    int result = cu_ModuleGetGlobal(&slang_params_ptr, &slang_params_size, module_->handle, "SLANG_globalParams");
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "Failed to get SLANG_globalParams symbol: " + CudaErrorToString(result));
    }
    
    // Validate parameter size
    if (size > slang_params_size) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::INVALID_ARGUMENT,
                                     "Parameter size (" + std::to_string(size) + 
                                     ") exceeds SLANG_globalParams size (" + std::to_string(slang_params_size) + ")");
    }
    
    // Copy parameters to constant memory
    result = cu_MemcpyHtoD(slang_params_ptr, params, size);
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "Failed to copy parameters to SLANG_globalParams: " + CudaErrorToString(result));
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Set SLANG global parameters: " + 
                        std::to_string(size) + " bytes to constant memory");
    
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> CudaKernelRunner::SetBuffer(int binding, std::shared_ptr<IBuffer> buffer) {
    auto cuda_buffer = std::dynamic_pointer_cast<CudaBuffer>(buffer);
    if (!cuda_buffer) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::VALIDATION, ErrorCode::INVALID_ARGUMENT,
                                     "Buffer is not a CUDA buffer");
    }
    
    buffer_bindings_[binding] = cuda_buffer->GetDevicePointer();
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> CudaKernelRunner::SetTexture(int binding, std::shared_ptr<ITexture> texture) {
    auto cuda_texture = std::dynamic_pointer_cast<CudaTexture>(texture);
    if (!cuda_texture) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::VALIDATION, ErrorCode::INVALID_ARGUMENT,
                                     "Texture is not a CUDA texture");
    }
    
    buffer_bindings_[binding] = cuda_texture->GetDevicePointer();
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> CudaKernelRunner::Dispatch(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) {
    if (!function_->handle) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "No kernel loaded");
    }
    
    // Ensure context is current
    cu_CtxSetCurrent(context_->handle);
    
    // Record start timing
    auto start_time = std::chrono::steady_clock::now();
    cu_EventRecord(start_event_->handle, nullptr);
    
    // Prepare kernel arguments (simplified - assuming buffer pointers)
    std::vector<void*> args;
    for (auto& binding : buffer_bindings_) {
        args.push_back(&binding.second);
    }
    
    // Launch kernel with 16x16 thread blocks (matching SLANG [numthreads(16, 16, 1)])
    int result = cu_LaunchKernel(
        function_->handle,
        groups_x, groups_y, groups_z,  // Grid dimensions
        16, 16, 1,                     // Block dimensions
        0,                             // Shared memory
        nullptr,                       // Stream
        args.empty() ? nullptr : args.data(),  // Kernel arguments
        nullptr                        // Extra
    );
    
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "Failed to launch CUDA kernel: " + CudaErrorToString(result));
    }
    
    // Record stop timing
    cu_EventRecord(stop_event_->handle, nullptr);
    auto end_time = std::chrono::steady_clock::now();
    
    // Store timing info
    last_timing_.start_time = start_time;
    last_timing_.end_time = end_time;
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Dispatched CUDA kernel: " + 
                       std::to_string(groups_x) + "x" + std::to_string(groups_y) + "x" + std::to_string(groups_z));
    
    return KERNTOPIA_VOID_SUCCESS();
}

Result<void> CudaKernelRunner::WaitForCompletion() {
    int result = cu_CtxSynchronize();
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_OPERATION_FAILED,
                                     "CUDA synchronization failed: " + CudaErrorToString(result));
    }
    
    // Calculate execution time
    float elapsed_ms = 0.0f;
    result = cu_EventElapsedTime(&elapsed_ms, start_event_->handle, stop_event_->handle);
    if (result == CUDA_SUCCESS) {
        last_timing_.compute_time_ms = elapsed_ms;
    }
    
    auto duration = last_timing_.end_time - last_timing_.start_time;
    last_timing_.total_time_ms = std::chrono::duration<float, std::milli>(duration).count();
    
    return KERNTOPIA_VOID_SUCCESS();
}

TimingResults CudaKernelRunner::GetLastExecutionTime() {
    return last_timing_;
}

Result<std::shared_ptr<IBuffer>> CudaKernelRunner::CreateBuffer(size_t size, IBuffer::Type type, IBuffer::Usage usage) {
    auto buffer = std::make_shared<CudaBuffer>(size, type, usage);
    if (!buffer->GetDevicePointer()) {
        return KERNTOPIA_RESULT_ERROR(std::shared_ptr<IBuffer>, ErrorCategory::BACKEND, 
                                     ErrorCode::MEMORY_ALLOCATION_FAILED, "Failed to allocate CUDA buffer");
    }
    
    return Result<std::shared_ptr<IBuffer>>::Success(buffer);
}

Result<std::shared_ptr<ITexture>> CudaKernelRunner::CreateTexture(const TextureDesc& desc) {
    auto texture = std::make_shared<CudaTexture>(desc);
    if (!texture->GetDevicePointer()) {
        return KERNTOPIA_RESULT_ERROR(std::shared_ptr<ITexture>, ErrorCategory::BACKEND,
                                     ErrorCode::MEMORY_ALLOCATION_FAILED, "Failed to allocate CUDA texture");
    }
    
    return Result<std::shared_ptr<ITexture>>::Success(texture);
}

void CudaKernelRunner::CalculateDispatchSize(uint32_t width, uint32_t height, uint32_t depth,
                                            uint32_t& groups_x, uint32_t& groups_y, uint32_t& groups_z) {
    // Calculate number of thread groups needed for 16x16 thread blocks
    groups_x = (width + 15) / 16;
    groups_y = (height + 15) / 16;
    groups_z = std::max(1u, depth);
}

std::string CudaKernelRunner::GetDebugInfo() const {
    std::ostringstream info;
    info << "CUDA Kernel Runner:\n";
    info << "  Device ID: " << device_id_ << "\n";
    info << "  Device Name: " << GetDeviceName() << "\n";
    info << "  Context: " << (context_->handle ? "Valid" : "Invalid") << "\n";
    info << "  Module: " << (module_->handle ? "Loaded" : "Not Loaded") << "\n";
    info << "  Function: " << (function_->handle ? "Ready" : "Not Ready") << "\n";
    info << "  Buffer Bindings: " << buffer_bindings_.size();
    return info.str();
}

bool CudaKernelRunner::SupportsFeature(const std::string& feature) const {
    if (feature == "compute") return true;
    if (feature == "timing") return true;
    if (feature == "ptx") return true;
    return false;
}


// Helper function to initialize CUDA driver using SystemInterrogator for library discovery
static Result<void> InitializeCudaDriver() {
    if (cuda_driver_handle) {
        return KERNTOPIA_VOID_SUCCESS(); // Already loaded
    }
    
    // Use SystemInterrogator to discover CUDA library paths (detection only)
    auto runtime_result = SystemInterrogator::GetRuntimeInfo(RuntimeType::CUDA);
    if (!runtime_result.HasValue()) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "SystemInterrogator failed to detect CUDA: " + runtime_result.GetError().message);
    }
    
    RuntimeInfo cuda_info = runtime_result.GetValue();
    if (!cuda_info.available) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "CUDA runtime not available: " + cuda_info.error_message);
    }
    
    // Load the CUDA driver library using the path discovered by SystemInterrogator
    RuntimeLoader loader;
    
    // Try the primary library path first, then fall back to library search
    std::vector<std::string> cuda_paths;
    if (!cuda_info.primary_library_path.empty()) {
        cuda_paths.push_back(cuda_info.primary_library_path);
    }
    // Add library search patterns as fallback
    cuda_paths.insert(cuda_paths.end(), {"cuda", "nvcuda", "libcuda", "libcuda.so", "libcuda.so.1"});
    
    Result<void*> load_result = Result<void*>::Error(ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED, "No CUDA library found");
    for (const std::string& lib_path : cuda_paths) {
        if (lib_path.find('/') != std::string::npos) {
            // Absolute path - load directly
            load_result = loader.LoadLibrary(lib_path);
        } else {
            // Library name - find then load
            auto find_result = loader.FindLibrary(lib_path);
            if (find_result.HasValue()) {
                load_result = loader.LoadLibrary(find_result.GetValue().full_path);
            }
        }
        
        if (load_result.HasValue()) {
            cuda_driver_handle = load_result.GetValue();
            KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Loaded CUDA driver: " + lib_path);
            break;
        }
    }
    
    if (!load_result.HasValue()) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to load CUDA driver library");
    }
    
    // Load required function pointers
    cu_Init = reinterpret_cast<cuInit_t>(loader.GetSymbol(cuda_driver_handle, "cuInit"));
    cu_DeviceGetCount = reinterpret_cast<cuDeviceGetCount_t>(loader.GetSymbol(cuda_driver_handle, "cuDeviceGetCount"));
    cu_DeviceGet = reinterpret_cast<cuDeviceGet_t>(loader.GetSymbol(cuda_driver_handle, "cuDeviceGet"));
    cu_DeviceGetName = reinterpret_cast<cuDeviceGetName_t>(loader.GetSymbol(cuda_driver_handle, "cuDeviceGetName"));
    cu_DeviceGetAttribute = reinterpret_cast<cuDeviceGetAttribute_t>(loader.GetSymbol(cuda_driver_handle, "cuDeviceGetAttribute"));
    cu_CtxCreate = reinterpret_cast<cuCtxCreate_t>(loader.GetSymbol(cuda_driver_handle, "cuCtxCreate_v2"));
    cu_CtxDestroy = reinterpret_cast<cuCtxDestroy_t>(loader.GetSymbol(cuda_driver_handle, "cuCtxDestroy_v2"));
    cu_CtxSetCurrent = reinterpret_cast<cuCtxSetCurrent_t>(loader.GetSymbol(cuda_driver_handle, "cuCtxSetCurrent"));
    cu_ModuleLoadData = reinterpret_cast<cuModuleLoadData_t>(loader.GetSymbol(cuda_driver_handle, "cuModuleLoadData"));
    cu_ModuleUnload = reinterpret_cast<cuModuleUnload_t>(loader.GetSymbol(cuda_driver_handle, "cuModuleUnload"));
    cu_ModuleGetFunction = reinterpret_cast<cuModuleGetFunction_t>(loader.GetSymbol(cuda_driver_handle, "cuModuleGetFunction"));
    cu_ModuleGetGlobal = reinterpret_cast<cuModuleGetGlobal_t>(loader.GetSymbol(cuda_driver_handle, "cuModuleGetGlobal_v2"));
    cu_MemAlloc = reinterpret_cast<cuMemAlloc_t>(loader.GetSymbol(cuda_driver_handle, "cuMemAlloc_v2"));
    cu_MemFree = reinterpret_cast<cuMemFree_t>(loader.GetSymbol(cuda_driver_handle, "cuMemFree_v2"));
    cu_MemcpyHtoD = reinterpret_cast<cuMemcpyHtoD_t>(loader.GetSymbol(cuda_driver_handle, "cuMemcpyHtoD_v2"));
    cu_MemcpyDtoH = reinterpret_cast<cuMemcpyDtoH_t>(loader.GetSymbol(cuda_driver_handle, "cuMemcpyDtoH_v2"));
    cu_LaunchKernel = reinterpret_cast<cuLaunchKernel_t>(loader.GetSymbol(cuda_driver_handle, "cuLaunchKernel"));
    cu_EventCreate = reinterpret_cast<cuEventCreate_t>(loader.GetSymbol(cuda_driver_handle, "cuEventCreate"));
    cu_EventDestroy = reinterpret_cast<cuEventDestroy_t>(loader.GetSymbol(cuda_driver_handle, "cuEventDestroy_v2"));
    cu_EventRecord = reinterpret_cast<cuEventRecord_t>(loader.GetSymbol(cuda_driver_handle, "cuEventRecord"));
    cu_EventElapsedTime = reinterpret_cast<cuEventElapsedTime_t>(loader.GetSymbol(cuda_driver_handle, "cuEventElapsedTime"));
    cu_CtxSynchronize = reinterpret_cast<cuCtxSynchronize_t>(loader.GetSymbol(cuda_driver_handle, "cuCtxSynchronize"));
    cu_GetErrorString = reinterpret_cast<cuGetErrorString_t>(loader.GetSymbol(cuda_driver_handle, "cuGetErrorString"));
    
    // Verify critical functions were loaded
    if (!cu_Init || !cu_DeviceGetCount || !cu_DeviceGet || !cu_CtxCreate || !cu_ModuleLoadData) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to load required CUDA driver functions");
    }
    
    // Initialize CUDA
    int result = cu_Init(0);
    if (result != CUDA_SUCCESS) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE,
                                     "CUDA initialization failed");
    }
    
    return KERNTOPIA_VOID_SUCCESS();
}


// CudaKernelRunnerFactory implementation
bool CudaKernelRunnerFactory::IsAvailable() const {
    // Use SystemInterrogator for consistent runtime detection
    return SystemInterrogator::IsRuntimeAvailable(RuntimeType::CUDA);
}

std::vector<DeviceInfo> CudaKernelRunnerFactory::EnumerateDevices() const {
    // Use SystemInterrogator to get the actual detected CUDA devices
    // This avoids duplicating device detection logic
    auto system_info_result = SystemInterrogator::GetSystemInfo();
    if (!system_info_result.HasValue()) {
        KERNTOPIA_LOG_WARNING(LogComponent::BACKEND, "SystemInterrogator failed to get system info: " + 
                             system_info_result.GetError().message);
        return {};
    }
    
    auto system_info = system_info_result.GetValue();
    
    // Extract CUDA devices from system info
    std::vector<DeviceInfo> devices = system_info.cuda_runtime.devices;
    
    if (devices.empty()) {
        KERNTOPIA_LOG_WARNING(LogComponent::BACKEND, "No CUDA devices found in system interrogation");
        return {};
    }
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Found " + std::to_string(devices.size()) + " CUDA devices via SystemInterrogator");
    
    for (size_t i = 0; i < devices.size(); ++i) {
        KERNTOPIA_LOG_INFO(LogComponent::BACKEND, 
            "CUDA Device " + std::to_string(i) + ": " + devices[i].name + 
            " (CC " + devices[i].compute_capability + ", " + 
            std::to_string(devices[i].total_memory_bytes / (1024*1024)) + " MB)");
    }
    
    return devices;
}

Result<std::unique_ptr<IKernelRunner>> CudaKernelRunnerFactory::CreateRunner(int device_id) const {
    // Use SystemInterrogator for basic availability check (detection only)
    if (!SystemInterrogator::IsRuntimeAvailable(RuntimeType::CUDA)) {
        return KERNTOPIA_RESULT_ERROR(std::unique_ptr<IKernelRunner>, ErrorCategory::BACKEND,
                                     ErrorCode::BACKEND_NOT_AVAILABLE, "CUDA runtime not available via SystemInterrogator");
    }
    
    // CUDA backend handles its own device enumeration and validation
    auto devices = EnumerateDevices();
    if (device_id < 0 || static_cast<size_t>(device_id) >= devices.size()) {
        return KERNTOPIA_RESULT_ERROR(std::unique_ptr<IKernelRunner>, ErrorCategory::VALIDATION,
                                     ErrorCode::INVALID_ARGUMENT, 
                                     "Invalid CUDA device ID: " + std::to_string(device_id) + 
                                     " (available: 0-" + std::to_string(devices.size() - 1) + ")");
    }
    
    // Create and initialize runner (driver already initialized by EnumerateDevices)
    std::unique_ptr<IKernelRunner> runner = std::make_unique<CudaKernelRunner>(device_id);
    
    KERNTOPIA_LOG_INFO(LogComponent::BACKEND, "Created CUDA kernel runner for device " + std::to_string(device_id) + 
                       " (" + devices[device_id].name + ")");
    return Result<std::unique_ptr<IKernelRunner>>::Success(std::move(runner));
}

std::string CudaKernelRunnerFactory::GetVersion() const {
    auto runtime_result = SystemInterrogator::GetRuntimeInfo(RuntimeType::CUDA);
    if (!runtime_result.HasValue()) {
        return "Not Available";
    }
    
    RuntimeInfo cuda_info = runtime_result.GetValue();
    return cuda_info.available ? cuda_info.version : "Not Available";
}

} // namespace kerntopia