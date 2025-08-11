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
 * @brief Configuration for individual kernel tests
 */
struct TestConfiguration {
    // Execution parameters
    Backend target_backend = Backend::CUDA;
    int device_id = 0;
    TestMode mode = TestMode::FUNCTIONAL;
    int iterations = 1;
    float timeout_seconds = 60.0f;
    
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
            case Backend::VULKAN: return "Vulkan";
            case Backend::CPU: return "CPU";
            case Backend::DX12: return "DirectX12";
            default: return "Unknown";
        }
    }
    
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