#pragma once

#include "../common/kernel_result.hpp"
#include "../common/test_params.hpp"
#include "../common/data_span.hpp"
#include "../common/error_handling.hpp"

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace kerntopia {

// Forward declarations
class IBuffer;
class ITexture;

/**
 * @brief GPU device information and capabilities
 */
struct DeviceInfo {
    int device_id = -1;                          ///< Device index
    std::string name;                            ///< Device name
    Backend backend_type;                        ///< Backend type
    
    // Memory information
    uint64_t total_memory_bytes = 0;             ///< Total device memory
    uint64_t free_memory_bytes = 0;              ///< Currently free memory
    
    // Compute capabilities
    std::string compute_capability;              ///< Compute capability (e.g., "7.5" for CUDA)
    uint32_t max_threads_per_group = 0;          ///< Maximum threads per workgroup
    uint32_t max_shared_memory_bytes = 0;        ///< Maximum shared memory per group
    
    // API support
    std::string api_version;                     ///< API version string
    std::vector<std::string> supported_extensions; ///< Supported extensions
    
    // Performance characteristics
    uint32_t multiprocessor_count = 0;           ///< Number of SMs/CUs
    uint32_t base_clock_mhz = 0;                 ///< Base clock frequency
    uint32_t boost_clock_mhz = 0;                ///< Boost clock frequency
    float memory_bandwidth_gbps = 0.0f;          ///< Memory bandwidth
    
    bool is_integrated = false;                  ///< True for integrated GPUs
    bool supports_compute = true;                ///< Compute shader support
    bool supports_graphics = false;              ///< Graphics support (not needed for Kerntopia)
    
    /**
     * @brief Check if device meets minimum requirements
     * 
     * @param min_memory_gb Minimum memory in GB
     * @param min_compute_capability Minimum compute capability
     * @return True if device is suitable
     */
    bool MeetsMinimumRequirements(float min_memory_gb = 2.0f, 
                                 const std::string& min_compute_capability = "") const {
        float memory_gb = static_cast<float>(total_memory_bytes) / (1024.0f * 1024.0f * 1024.0f);
        if (memory_gb < min_memory_gb) {
            return false;
        }
        
        // Additional backend-specific capability checks can be added here
        return supports_compute;
    }
};

/**
 * @brief Abstract GPU buffer interface for cross-backend compatibility
 */
class IBuffer {
public:
    enum class Type {
        VERTEX,        ///< Vertex buffer
        INDEX,         ///< Index buffer  
        UNIFORM,       ///< Uniform/constant buffer
        STORAGE,       ///< Storage buffer (read/write)
        STAGING        ///< Staging buffer for CPU access
    };
    
    enum class Usage {
        STATIC,        ///< Written once, read many times
        DYNAMIC,       ///< Updated frequently
        STREAM         ///< Written once per frame
    };

    virtual ~IBuffer() = default;
    
    /**
     * @brief Get buffer size in bytes
     * 
     * @return Buffer size
     */
    virtual size_t GetSize() const = 0;
    
    /**
     * @brief Get buffer type
     * 
     * @return Buffer type
     */
    virtual Type GetType() const = 0;
    
    /**
     * @brief Map buffer memory for CPU access
     * 
     * @return Pointer to mapped memory or nullptr on failure
     */
    virtual void* Map() = 0;
    
    /**
     * @brief Unmap buffer memory
     */
    virtual void Unmap() = 0;
    
    /**
     * @brief Upload data to buffer
     * 
     * @param data Source data
     * @param size Data size in bytes
     * @param offset Offset in buffer
     * @return Success result
     */
    virtual Result<void> UploadData(const void* data, size_t size, size_t offset = 0) = 0;
    
    /**
     * @brief Download data from buffer
     * 
     * @param data Destination data
     * @param size Data size in bytes  
     * @param offset Offset in buffer
     * @return Success result
     */
    virtual Result<void> DownloadData(void* data, size_t size, size_t offset = 0) = 0;
};

/**
 * @brief Texture description for creation
 */
struct TextureDesc {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;                         ///< Depth for 3D textures
    uint32_t mip_levels = 1;                    ///< Number of mip levels
    uint32_t array_layers = 1;                  ///< Array layers for texture arrays
    
    enum class Format {
        R8_UNORM,      ///< 8-bit single channel
        RG8_UNORM,     ///< 8-bit dual channel
        RGBA8_UNORM,   ///< 8-bit RGBA
        R16_FLOAT,     ///< 16-bit float single channel
        RGBA16_FLOAT,  ///< 16-bit float RGBA
        R32_FLOAT,     ///< 32-bit float single channel
        RGBA32_FLOAT   ///< 32-bit float RGBA
    } format = Format::RGBA8_UNORM;
    
    enum class Type {
        TEXTURE_1D,
        TEXTURE_2D,
        TEXTURE_3D,
        TEXTURE_CUBE
    } type = Type::TEXTURE_2D;
    
    bool is_render_target = false;              ///< Can be used as render target
    bool is_storage = false;                    ///< Can be used for compute writes
    bool generate_mips = false;                 ///< Generate mipmaps automatically
};

/**
 * @brief Abstract GPU texture interface
 */
class ITexture {
public:
    virtual ~ITexture() = default;
    
    /**
     * @brief Get texture description
     * 
     * @return Texture description
     */
    virtual const TextureDesc& GetDesc() const = 0;
    
    /**
     * @brief Upload image data to texture
     * 
     * @param data Image data
     * @param mip_level Mip level to update
     * @param array_layer Array layer to update
     * @return Success result
     */
    virtual Result<void> UploadData(const void* data, uint32_t mip_level = 0, uint32_t array_layer = 0) = 0;
    
    /**
     * @brief Download image data from texture
     * 
     * @param data Destination buffer
     * @param data_size Buffer size
     * @param mip_level Mip level to read
     * @param array_layer Array layer to read
     * @return Success result
     */
    virtual Result<void> DownloadData(void* data, size_t data_size, uint32_t mip_level = 0, uint32_t array_layer = 0) = 0;
};

/**
 * @brief Abstract kernel runner interface for cross-backend GPU execution
 * 
 * Provides unified interface for executing compute kernels across different GPU APIs.
 * Each backend (CUDA, Vulkan, etc.) implements this interface to provide consistent
 * kernel execution capabilities with performance timing and resource management.
 */
class IKernelRunner {
public:
    virtual ~IKernelRunner() = default;
    
    // Device and backend information
    
    /**
     * @brief Get backend name (e.g., "CUDA", "Vulkan")
     * 
     * @return Backend name string
     */
    virtual std::string GetBackendName() const = 0;
    
    /**
     * @brief Get device name (e.g., "GeForce RTX 4060")
     * 
     * @return Device name string
     */
    virtual std::string GetDeviceName() const = 0;
    
    /**
     * @brief Get device capabilities and information
     * 
     * @return Device information structure
     */
    virtual DeviceInfo GetDeviceInfo() const = 0;
    
    // Kernel lifecycle management
    
    /**
     * @brief Load compiled kernel bytecode
     * 
     * @param bytecode Backend-specific compiled bytecode (PTX, SPIRV, etc.)
     * @param entry_point Kernel entry point function name
     * @return Success result
     */
    virtual Result<void> LoadKernel(const std::vector<uint8_t>& bytecode, 
                                   const std::string& entry_point) = 0;
    
    /**
     * @brief Set uniform/constant parameters for kernel
     * 
     * @param params Pointer to parameter structure
     * @param size Size of parameter structure
     * @return Success result
     */
    virtual Result<void> SetParameters(const void* params, size_t size) = 0;
    
    /**
     * @brief Bind buffer to kernel parameter binding point
     * 
     * @param binding Binding point index
     * @param buffer Buffer to bind
     * @return Success result
     */
    virtual Result<void> SetBuffer(int binding, std::shared_ptr<IBuffer> buffer) = 0;
    
    /**
     * @brief Bind texture to kernel parameter binding point
     * 
     * @param binding Binding point index
     * @param texture Texture to bind
     * @return Success result
     */
    virtual Result<void> SetTexture(int binding, std::shared_ptr<ITexture> texture) = 0;
    
    // Execution and timing
    
    /**
     * @brief Dispatch kernel with specified thread group dimensions
     * 
     * @param groups_x Number of thread groups in X dimension
     * @param groups_y Number of thread groups in Y dimension
     * @param groups_z Number of thread groups in Z dimension
     * @return Success result
     */
    virtual Result<void> Dispatch(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) = 0;
    
    /**
     * @brief Wait for kernel execution completion
     * 
     * @return Success result
     */
    virtual Result<void> WaitForCompletion() = 0;
    
    /**
     * @brief Get timing information from last kernel execution
     * 
     * @return Timing results with separated memory and compute phases
     */
    virtual TimingResults GetLastExecutionTime() = 0;
    
    // Memory management
    
    /**
     * @brief Create buffer with specified size and type
     * 
     * @param size Buffer size in bytes
     * @param type Buffer type and usage pattern
     * @param usage Usage hint for optimization
     * @return Buffer instance or error
     */
    virtual Result<std::shared_ptr<IBuffer>> CreateBuffer(size_t size, 
                                                         IBuffer::Type type,
                                                         IBuffer::Usage usage = IBuffer::Usage::STATIC) = 0;
    
    /**
     * @brief Create texture with specified description
     * 
     * @param desc Texture description
     * @return Texture instance or error
     */
    virtual Result<std::shared_ptr<ITexture>> CreateTexture(const TextureDesc& desc) = 0;
    
    // Utility functions
    
    /**
     * @brief Calculate optimal thread group dimensions for given problem size
     * 
     * @param width Problem width
     * @param height Problem height
     * @param depth Problem depth
     * @param groups_x Output groups in X
     * @param groups_y Output groups in Y
     * @param groups_z Output groups in Z
     */
    virtual void CalculateDispatchSize(uint32_t width, uint32_t height, uint32_t depth,
                                      uint32_t& groups_x, uint32_t& groups_y, uint32_t& groups_z) = 0;
    
    /**
     * @brief Get backend-specific information for debugging
     * 
     * @return Debug information string
     */
    virtual std::string GetDebugInfo() const = 0;
    
    /**
     * @brief Check if backend supports specific features
     * 
     * @param feature Feature name to check
     * @return True if feature is supported
     */
    virtual bool SupportsFeature(const std::string& feature) const = 0;
    
    /**
     * @brief Set SLANG global parameters for kernel execution
     * 
     * This method provides a unified interface for setting SLANG-generated
     * global parameters across different backends (CUDA constant memory,
     * Vulkan descriptor sets, etc.)
     * 
     * @param params Pointer to parameter data buffer
     * @param size Size of parameter data in bytes
     * @return Success result
     */
    virtual Result<void> SetSlangGlobalParameters(const void* params, size_t size) = 0;
};

/**
 * @brief Factory interface for creating kernel runners
 */
class IKernelRunnerFactory {
public:
    virtual ~IKernelRunnerFactory() = default;
    
    /**
     * @brief Check if backend is available on this system
     * 
     * @return True if backend can be used
     */
    virtual bool IsAvailable() const = 0;
    
    /**
     * @brief Enumerate available devices for this backend
     * 
     * @return List of available devices
     */
    virtual std::vector<DeviceInfo> EnumerateDevices() const = 0;
    
    /**
     * @brief Create kernel runner for specified device
     * 
     * @param device_id Device index to use
     * @return Kernel runner instance or error
     */
    virtual Result<std::unique_ptr<IKernelRunner>> CreateRunner(int device_id = 0) const = 0;
    
    /**
     * @brief Get backend type
     * 
     * @return Backend type enum
     */
    virtual Backend GetBackendType() const = 0;
    
    /**
     * @brief Get backend version information
     * 
     * @return Version string
     */
    virtual std::string GetVersion() const = 0;
};

} // namespace kerntopia