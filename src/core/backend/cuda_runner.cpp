#include "cuda_runner.hpp"
#include "../common/logger.hpp"
#include "runtime_loader.hpp"
#include "../system/system_interrogator.hpp"
#include "../system/interrogation_data.hpp"

#include <sstream>
#include <algorithm>
#include <cstring>

// CUDA Driver API - Require SDK headers
#ifdef KERNTOPIA_CUDA_SDK_AVAILABLE
#include <cuda.h>
// Use official CUDA types and function signatures
typedef CUresult (*cuInit_t)(unsigned int Flags);
typedef CUresult (*cuDeviceGetCount_t)(int* count);
typedef CUresult (*cuDeviceGet_t)(CUdevice* device, int ordinal);
typedef CUresult (*cuDeviceGetName_t)(char* name, int len, CUdevice dev);
typedef CUresult (*cuDeviceGetAttribute_t)(int* pi, CUdevice_attribute attrib, CUdevice dev);
typedef CUresult (*cuCtxCreate_t)(CUcontext* pctx, unsigned int flags, CUdevice dev);
typedef CUresult (*cuCtxDestroy_t)(CUcontext ctx);
typedef CUresult (*cuCtxSetCurrent_t)(CUcontext ctx);
typedef CUresult (*cuModuleLoadData_t)(CUmodule* module, const void* image);
typedef CUresult (*cuModuleUnload_t)(CUmodule hmod);
typedef CUresult (*cuModuleGetFunction_t)(CUfunction* hfunc, CUmodule hmod, const char* name);
typedef CUresult (*cuModuleGetGlobal_t)(CUdeviceptr* dptr, size_t* bytes, CUmodule hmod, const char* name);
typedef CUresult (*cuMemAlloc_t)(CUdeviceptr* dptr, size_t bytesize);
typedef CUresult (*cuMemFree_t)(CUdeviceptr dptr);
typedef CUresult (*cuMemcpyHtoD_t)(CUdeviceptr dstDevice, const void* srcHost, size_t ByteCount);
typedef CUresult (*cuMemcpyDtoH_t)(void* dstHost, CUdeviceptr srcDevice, size_t ByteCount);
typedef CUresult (*cuLaunchKernel_t)(CUfunction f, unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ, 
                                   unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ,
                                   unsigned int sharedMemBytes, CUstream hStream, void** kernelParams, void** extra);
typedef CUresult (*cuEventCreate_t)(CUevent* phEvent, unsigned int Flags);
typedef CUresult (*cuEventDestroy_t)(CUevent hEvent);
typedef CUresult (*cuEventRecord_t)(CUevent hEvent, CUstream hStream);
typedef CUresult (*cuEventElapsedTime_t)(float* pMilliseconds, CUevent hStart, CUevent hEnd);
typedef CUresult (*cuCtxSynchronize_t)(void);
typedef CUresult (*cuGetErrorString_t)(CUresult error, const char** pStr);
#else
#error "CUDA SDK headers are required but not found. Please install CUDA SDK or set CUDA_SDK environment variable."
#endif

namespace kerntopia {

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
// Memory function pointers are now defined in cuda_memory.cpp as extern
static cuLaunchKernel_t cu_LaunchKernel = nullptr;
static cuEventCreate_t cu_EventCreate = nullptr;
static cuEventDestroy_t cu_EventDestroy = nullptr;
static cuEventRecord_t cu_EventRecord = nullptr;
static cuEventElapsedTime_t cu_EventElapsedTime = nullptr;
static cuCtxSynchronize_t cu_CtxSynchronize = nullptr;
// cu_GetErrorString is now defined in cuda_memory.cpp as extern

static LibraryHandle cuda_driver_handle = nullptr;

// CUDA error codes and device attributes are now provided by the SDK headers

// CUDA context structures using proper CUDA types
struct CudaContext {
    CUcontext handle = nullptr;
};

struct CudaModule {
    CUmodule handle = nullptr;
};

struct CudaFunction {
    CUfunction handle = nullptr;
};

struct CudaEvent {
    CUevent handle = nullptr;
};


// Helper function to convert CUDA error to string
std::string CudaKernelRunner::CudaErrorToString(CUresult cuda_error) {
    if (cu_GetErrorString) {
        const char* error_str = nullptr;
        cu_GetErrorString(cuda_error, &error_str);
        return error_str ? std::string(error_str) : "Unknown CUDA error";
    }
    return "CUDA error " + std::to_string(static_cast<int>(cuda_error));
}

// Memory class implementations moved to cuda_memory.cpp

// CudaKernelRunner implementation
CudaKernelRunner::CudaKernelRunner(int device_id, const DeviceInfo& device_info) 
    : device_id_(device_id), device_info_(device_info) {
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
    CUdevice device;
    CUresult result = cu_DeviceGet(&device, device_id_);
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
    CUresult result = cu_EventCreate(&start_event_->handle, 0);
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
    // Return cached device info from SystemInterrogator (follows Vulkan pattern)
    return device_info_;
}

Result<void> CudaKernelRunner::LoadKernel(const std::vector<uint8_t>& bytecode, const std::string& entry_point) {
    // Ensure context is current
    cu_CtxSetCurrent(context_->handle);
    
    // Load PTX module
    CUresult result = cu_ModuleLoadData(&module_->handle, bytecode.data());
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
    CUdeviceptr slang_params_ptr;
    size_t slang_params_size;
    CUresult result = cu_ModuleGetGlobal(&slang_params_ptr, &slang_params_size, module_->handle, "SLANG_globalParams");
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
    CUresult result = cu_LaunchKernel(
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
    CUresult result = cu_CtxSynchronize();
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
    RuntimeLoader& loader = RuntimeLoader::GetInstance();
    
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
    
    // Initialize extern function pointers for cuda_memory.cpp
    // (they reference the same loaded functions)
    kerntopia::cu_MemAlloc = cu_MemAlloc;
    kerntopia::cu_MemFree = cu_MemFree;
    kerntopia::cu_MemcpyHtoD = cu_MemcpyHtoD;
    kerntopia::cu_MemcpyDtoH = cu_MemcpyDtoH;
    kerntopia::cu_GetErrorString = cu_GetErrorString;
    
    // Verify critical functions were loaded
    if (!cu_Init || !cu_DeviceGetCount || !cu_DeviceGet || !cu_CtxCreate || !cu_ModuleLoadData || !cu_GetErrorString) {
        std::string missing_functions;
        if (!cu_Init) missing_functions += "cuInit ";
        if (!cu_DeviceGetCount) missing_functions += "cuDeviceGetCount ";
        if (!cu_DeviceGet) missing_functions += "cuDeviceGet ";
        if (!cu_CtxCreate) missing_functions += "cuCtxCreate ";
        if (!cu_ModuleLoadData) missing_functions += "cuModuleLoadData ";
        if (!cu_GetErrorString) missing_functions += "cuGetErrorString ";
        
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to load required CUDA driver functions: " + missing_functions);
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "All critical CUDA functions loaded successfully");
    
    // Initialize CUDA
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "Calling cuInit(0)...");
    CUresult result = cu_Init(0);
    if (result != CUDA_SUCCESS) {
        // Get detailed error information
        std::string error_msg = "CUDA initialization failed with error code " + std::to_string(result);
        
        if (cu_GetErrorString) {
            const char* error_str = nullptr;
            CUresult string_result = cu_GetErrorString(result, &error_str);
            if (string_result == CUDA_SUCCESS && error_str) {
                error_msg += ": " + std::string(error_str);
            } else {
                error_msg += " (failed to get error string)";
            }
        } else {
            error_msg += " (cuGetErrorString not available)";
        }
        
        KERNTOPIA_LOG_ERROR(LogComponent::BACKEND, error_msg);
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::BACKEND, ErrorCode::BACKEND_NOT_AVAILABLE, error_msg);
    }
    
    KERNTOPIA_LOG_DEBUG(LogComponent::BACKEND, "cuInit(0) completed successfully");
    
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
    
    // Initialize CUDA driver before creating the runner
    auto driver_init_result = InitializeCudaDriver();
    if (!driver_init_result.HasValue()) {
        return KERNTOPIA_RESULT_ERROR(std::unique_ptr<IKernelRunner>, ErrorCategory::BACKEND,
                                     ErrorCode::BACKEND_NOT_AVAILABLE, 
                                     "Failed to initialize CUDA driver: " + driver_init_result.GetError().message);
    }
    
    // Create and initialize runner (driver now properly initialized)
    std::unique_ptr<IKernelRunner> runner = std::make_unique<CudaKernelRunner>(device_id, devices[device_id]);
    
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