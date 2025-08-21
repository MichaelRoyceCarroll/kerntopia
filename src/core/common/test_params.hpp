#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace kerntopia {

/**
 * @brief GPU backend types supported by Kerntopia
 */
enum class Backend {
    CUDA,       ///< NVIDIA CUDA backend
    VULKAN,     ///< Vulkan compute backend
    CPU,        ///< CPU fallback (via Vulkan llvmpipe)
    DX12        ///< DirectX 12 compute (future)
};

/**
 * @brief Test execution modes
 */
enum class TestMode {
    FUNCTIONAL,  ///< Single iteration functional test
    PERFORMANCE  ///< Multiple iterations with statistical analysis
};

/**
 * @brief Standard test image sizes
 */
enum class TestSize {
    HD_1080P,    ///< 1920x1080 resolution
    UHD_4K,      ///< 3840x2160 resolution  
    CUSTOM       ///< Custom size specified in parameters
};

/**
 * @brief Image formats supported for testing
 */
enum class ImageFormat {
    RGB8,        ///< 8-bit RGB
    RGB16,       ///< 16-bit RGB
    RGB32F,      ///< 32-bit float RGB
    YUV420P,     ///< YUV 4:2:0 planar
    YUV422,      ///< YUV 4:2:2
    YUV420P10,   ///< 10-bit YUV 4:2:0
    RAW12,       ///< 12-bit raw sensor data
    HDR_EXR      ///< OpenEXR HDR format
};

/**
 * @brief SLANG compilation profiles for different backends
 */
enum class SlangProfile {
    GLSL_450,     ///< GLSL 4.50 profile for Vulkan
    CUDA_SM_6_0,  ///< CUDA Compute Capability 6.0 (GTX 1060+, broadly compatible)
    CUDA_SM_7_0,  ///< CUDA Compute Capability 7.0 (GTX 1650+, RTX 20xx+) 
    CUDA_SM_8_0,  ///< CUDA Compute Capability 8.0 (RTX 4060+)
    HLSL_6_0,     ///< HLSL 6.0 for DirectX 12 (future)
    DEFAULT       ///< Auto-select based on backend
};

/**
 * @brief SLANG compilation targets
 */
enum class SlangTarget {
    SPIRV,        ///< SPIR-V bytecode for Vulkan
    PTX,          ///< PTX assembly for CUDA  
    GLSL,         ///< GLSL source code
    HLSL,         ///< HLSL source code (future)
    AUTO          ///< Auto-select based on backend
};

/**
 * @brief SLANG compilation modes
 */
enum class CompilationMode {
    PRECOMPILED,  ///< Use pre-built kernels from build process
    JIT           ///< Just-in-time compilation (future)
};

/**
 * @brief Configuration for individual kernel tests
 */
struct TestConfiguration {
    // Execution parameters
    Backend target_backend = Backend::CUDA;
    int device_id = 0;
    TestMode mode = TestMode::FUNCTIONAL;
    int iterations = 1;
    float timeout_seconds = 60.0f;
    
    // SLANG compilation parameters
    SlangProfile slang_profile = SlangProfile::DEFAULT;
    SlangTarget slang_target = SlangTarget::AUTO;
    CompilationMode compilation_mode = CompilationMode::PRECOMPILED;
    
    // Input parameters
    TestSize size = TestSize::HD_1080P;
    ImageFormat format = ImageFormat::RGB8;
    uint32_t custom_width = 0;
    uint32_t custom_height = 0;
    
    // Validation parameters
    bool validate_output = true;
    float validation_tolerance = 1e-6f;
    std::string reference_data_path;
    
    // Output parameters
    bool save_output = false;
    std::string output_path;
    bool save_intermediates = false;
    std::string temp_dir = "./temp";
    
    // Kernel-specific parameters (flexible key-value store)
    std::map<std::string, float> float_params;
    std::map<std::string, int> int_params;  
    std::map<std::string, std::string> string_params;
    
    /**
     * @brief Get actual image dimensions based on size setting
     * 
     * @param width Output width
     * @param height Output height
     */
    void GetImageDimensions(uint32_t& width, uint32_t& height) const {
        switch (size) {
            case TestSize::HD_1080P:
                width = 1920;
                height = 1080;
                break;
            case TestSize::UHD_4K:
                width = 3840;
                height = 2160;
                break;
            case TestSize::CUSTOM:
                width = custom_width;
                height = custom_height;
                break;
        }
    }
    
    /**
     * @brief Get backend name as string
     * 
     * @return Backend name
     */
    std::string GetBackendName() const {
        switch (target_backend) {
            case Backend::CUDA: return "CUDA";
            case Backend::VULKAN: return "VULKAN";
            case Backend::CPU: return "CPU";
            case Backend::DX12: return "DirectX12";
            default: return "Unknown";
        }
    }
    
    /**
     * @brief Get SLANG profile name as string
     * 
     * @return Profile name
     */
    std::string GetSlangProfileName() const {
        switch (slang_profile) {
            case SlangProfile::GLSL_450: return "glsl_450";
            case SlangProfile::CUDA_SM_6_0: return "cuda_sm_6_0";
            case SlangProfile::CUDA_SM_7_0: return "cuda_sm_7_0";
            case SlangProfile::CUDA_SM_8_0: return "cuda_sm_8_0";
            case SlangProfile::HLSL_6_0: return "hlsl_6_0";
            case SlangProfile::DEFAULT: return GetDefaultSlangProfile();
            default: return "unknown";
        }
    }
    
    /**
     * @brief Get SLANG target name as string
     * 
     * @return Target name
     */
    std::string GetSlangTargetName() const {
        switch (slang_target) {
            case SlangTarget::SPIRV: return "spirv";
            case SlangTarget::PTX: return "ptx";
            case SlangTarget::GLSL: return "glsl";
            case SlangTarget::HLSL: return "hlsl";
            case SlangTarget::AUTO: return GetDefaultSlangTarget();
            default: return "unknown";
        }
    }
    
    /**
     * @brief Get compilation mode name as string
     * 
     * @return Compilation mode name
     */
    std::string GetCompilationModeName() const {
        switch (compilation_mode) {
            case CompilationMode::PRECOMPILED: return "precompiled";
            case CompilationMode::JIT: return "jit";
            default: return "unknown";
        }
    }
    
    /**
     * @brief Get compiled kernel filename
     * 
     * @param kernel_name Base kernel name (e.g., "conv2d")
     * @return Compiled kernel filename (e.g., "conv2d-cuda_sm_7_0.ptx", "conv2d-glsl_450.spirv")
     */
    std::string GetCompiledKernelFilename(const std::string& kernel_name) const {
        std::string profile = GetSlangProfileName();
        std::string target = GetSlangTargetName();
        std::string extension = (target == "spirv") ? "spirv" : 
                               (target == "ptx") ? "ptx" : target;
        
        // Simplified naming: conv2d-cuda_sm_7_0.ptx, conv2d-glsl_450.spirv
        return kernel_name + "-" + profile + "." + extension;
    }
    
private:
    /**
     * @brief Get default SLANG profile for current backend
     */
    std::string GetDefaultSlangProfile() const {
        switch (target_backend) {
            case Backend::VULKAN:
            case Backend::CPU: return "glsl_450";
            case Backend::CUDA: return "cuda_sm_7_0";
            case Backend::DX12: return "hlsl_6_0";
            default: return "glsl_450";
        }
    }
    
    /**
     * @brief Get default SLANG target for current backend
     */
    std::string GetDefaultSlangTarget() const {
        switch (target_backend) {
            case Backend::VULKAN:
            case Backend::CPU: return "spirv";
            case Backend::CUDA: return "ptx";
            case Backend::DX12: return "hlsl";
            default: return "spirv";
        }
    }
    
public:
    
    /**
     * @brief Get test mode name as string
     * 
     * @return Test mode name
     */
    std::string GetModeName() const {
        switch (mode) {
            case TestMode::FUNCTIONAL: return "Functional";
            case TestMode::PERFORMANCE: return "Performance";
            default: return "Unknown";
        }
    }
    
    /**
     * @brief Get image format name as string
     * 
     * @return Format name
     */
    std::string GetFormatName() const {
        switch (format) {
            case ImageFormat::RGB8: return "RGB8";
            case ImageFormat::RGB16: return "RGB16";
            case ImageFormat::RGB32F: return "RGB32F";
            case ImageFormat::YUV420P: return "YUV420P";
            case ImageFormat::YUV422: return "YUV422";
            case ImageFormat::YUV420P10: return "YUV420P10";
            case ImageFormat::RAW12: return "RAW12";
            case ImageFormat::HDR_EXR: return "HDR_EXR";
            default: return "Unknown";
        }
    }
    
    /**
     * @brief Set kernel-specific parameter
     * 
     * @param name Parameter name
     * @param value Parameter value
     */
    void SetParam(const std::string& name, float value) {
        float_params[name] = value;
    }
    
    void SetParam(const std::string& name, int value) {
        int_params[name] = value;
    }
    
    void SetParam(const std::string& name, const std::string& value) {
        string_params[name] = value;
    }
    
    /**
     * @brief Get kernel-specific parameter
     * 
     * @param name Parameter name
     * @param default_value Default value if not found
     * @return Parameter value or default
     */
    template<typename T>
    T GetParam(const std::string& name, T default_value) const;
    
    /**
     * @brief Check if parameter exists
     * 
     * @param name Parameter name
     * @return True if parameter exists
     */
    bool HasParam(const std::string& name) const {
        return float_params.count(name) > 0 || 
               int_params.count(name) > 0 || 
               string_params.count(name) > 0;
    }
    
    /**
     * @brief Get output filename prefix for disambiguating test results
     * 
     * @return Prefix string like "CUDA_cuda_sm_7_0_ptx_Device_0"
     */
    std::string GetOutputPrefix() const {
        return GetBackendName() + "_" + 
               GetSlangProfileName() + "_" + 
               GetSlangTargetName() + "_Device_" + 
               std::to_string(device_id);
    }
};

// Template specializations for GetParam
template<>
inline float TestConfiguration::GetParam<float>(const std::string& name, float default_value) const {
    auto it = float_params.find(name);
    return (it != float_params.end()) ? it->second : default_value;
}

template<>
inline int TestConfiguration::GetParam<int>(const std::string& name, int default_value) const {
    auto it = int_params.find(name);
    return (it != int_params.end()) ? it->second : default_value;
}

template<>
inline std::string TestConfiguration::GetParam<std::string>(const std::string& name, std::string default_value) const {
    auto it = string_params.find(name);
    return (it != string_params.end()) ? it->second : default_value;
}

/**
 * @brief Global test suite configuration
 */
struct SuiteConfiguration {
    // Backend selection
    std::vector<Backend> target_backends = {Backend::CUDA, Backend::VULKAN};
    std::vector<int> device_ids = {0}; // Use device 0 by default
    
    // Test execution
    TestMode default_mode = TestMode::FUNCTIONAL;
    int performance_iterations = 10;
    float default_timeout_seconds = 300.0f;
    
    // Output and logging
    std::string output_directory = "./results";
    std::string log_file_path = "./kerntopia.log";
    bool verbose = false;
    bool save_intermediates = false;
    
    // Validation
    bool strict_validation = false;
    bool fail_on_validation_error = true;
    
    /**
     * @brief Get list of backend names
     * 
     * @return Vector of backend name strings
     */
    std::vector<std::string> GetBackendNames() const {
        std::vector<std::string> names;
        for (Backend backend : target_backends) {
            TestConfiguration temp_config;
            temp_config.target_backend = backend;
            names.push_back(temp_config.GetBackendName());
        }
        return names;
    }
};

} // namespace kerntopia