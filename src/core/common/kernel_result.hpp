#pragma once

#include "error_handling.hpp"
#include <chrono>
#include <map>
#include <string>

namespace kerntopia {

/**
 * @brief Timing information for kernel execution phases
 */
struct TimingResults {
    float memory_setup_time_ms = 0.0f;      ///< Time to allocate and transfer memory
    float compute_time_ms = 0.0f;           ///< Pure kernel execution time  
    float memory_teardown_time_ms = 0.0f;   ///< Time to read back results and cleanup
    float total_time_ms = 0.0f;             ///< Total end-to-end time
    
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    
    /**
     * @brief Calculate derived performance metrics
     * 
     * @param operations_count Number of operations performed
     * @return GFLOPS performance metric
     */
    float CalculateGFLOPS(uint64_t operations_count) const {
        if (compute_time_ms <= 0.0f) return 0.0f;
        return static_cast<float>(operations_count) / (compute_time_ms / 1000.0f) / 1e9f;
    }
    
    /**
     * @brief Calculate memory bandwidth utilization
     * 
     * @param bytes_transferred Total bytes read/written
     * @return Bandwidth in GB/s
     */
    float CalculateBandwidthGBps(uint64_t bytes_transferred) const {
        float total_transfer_time = memory_setup_time_ms + memory_teardown_time_ms;
        if (total_transfer_time <= 0.0f) return 0.0f;
        return static_cast<float>(bytes_transferred) / (total_transfer_time / 1000.0f) / 1e9f;
    }
};

/**
 * @brief Validation results for kernel output
 */
struct ValidationResults {
    bool passed = false;                    ///< Overall validation status
    float tolerance = 0.0f;                 ///< Tolerance used for comparison
    float max_difference = 0.0f;            ///< Maximum difference found
    float mean_difference = 0.0f;           ///< Mean difference across samples
    float psnr_db = 0.0f;                   ///< PSNR for image comparisons
    std::string validation_method;          ///< Method used for validation
    std::map<std::string, float> metrics;   ///< Additional validation metrics
};

/**
 * @brief Complete result of kernel execution
 */
struct KernelResult {
    bool success = false;                   ///< Overall execution success
    std::string kernel_name;                ///< Name of executed kernel
    std::string backend_name;               ///< Backend used for execution
    std::string device_name;                ///< Device name
    
    TimingResults timing;                   ///< Performance timing data
    ValidationResults validation;           ///< Output validation results
    
    std::map<std::string, float> metrics;   ///< Kernel-specific metrics
    ErrorInfo error{ErrorCategory::GENERAL, ErrorCode::SUCCESS, ""}; ///< Error information if failed
    
    // Execution metadata
    std::string slang_version;              ///< SLANG compiler version used
    std::string bytecode_checksum;          ///< Checksum of compiled bytecode  
    std::string input_checksum;             ///< Checksum of input data
    std::string output_checksum;            ///< Checksum of output data
    std::chrono::system_clock::time_point timestamp; ///< Execution timestamp
    
    /**
     * @brief Add custom metric to result
     * 
     * @param name Metric name
     * @param value Metric value
     */
    void AddMetric(const std::string& name, float value) {
        metrics[name] = value;
    }
    
    /**
     * @brief Get metric value
     * 
     * @param name Metric name
     * @return Metric value or 0.0f if not found
     */
    float GetMetric(const std::string& name) const {
        auto it = metrics.find(name);
        return (it != metrics.end()) ? it->second : 0.0f;
    }
    
    /**
     * @brief Check if kernel execution was successful
     * 
     * @return True if execution and validation passed
     */
    bool IsValid() const {
        return success && (validation.passed || validation.validation_method.empty());
    }
};

/**
 * @brief Statistical summary for multiple kernel executions
 */
struct StatisticalSummary {
    size_t sample_count = 0;
    float mean_time_ms = 0.0f;
    float std_deviation_ms = 0.0f;
    float min_time_ms = 0.0f;
    float max_time_ms = 0.0f;
    float median_time_ms = 0.0f;
    float coefficient_of_variation = 0.0f;
    
    // Performance metrics
    float mean_gflops = 0.0f;
    float mean_bandwidth_gbps = 0.0f;
    
    // Validation statistics
    float validation_pass_rate = 0.0f;
    float mean_validation_error = 0.0f;
    
    /**
     * @brief Check if performance is consistent
     * 
     * @param max_cv Maximum acceptable coefficient of variation
     * @return True if performance is consistent
     */
    bool IsPerformanceConsistent(float max_cv = 0.1f) const {
        return coefficient_of_variation <= max_cv;
    }
};

} // namespace kerntopia