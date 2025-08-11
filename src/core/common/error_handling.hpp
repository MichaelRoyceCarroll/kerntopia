#pragma once

#include <string>
#include <system_error>
#include <exception>
#include <vector>

namespace kerntopia {

/**
 * @brief Error categories for Kerntopia operations
 */
enum class ErrorCategory {
    GENERAL,         ///< General application errors
    BACKEND,         ///< GPU backend and device errors
    SLANG_COMPILE,   ///< SLANG compilation errors
    IMAGING,         ///< Image processing and I/O errors
    SYSTEM,          ///< System interrogation errors
    TEST,            ///< Test framework and execution errors
    VALIDATION       ///< Input validation and parameter errors
};

/**
 * @brief Specific error codes within categories
 */
enum class ErrorCode {
    SUCCESS = 0,
    
    // General errors (1-99)
    UNKNOWN_ERROR = 1,
    INVALID_ARGUMENT = 2,
    OUT_OF_MEMORY = 3,
    FILE_NOT_FOUND = 4,
    PERMISSION_DENIED = 5,
    OPERATION_CANCELLED = 6,
    
    // Backend errors (100-199)
    BACKEND_NOT_AVAILABLE = 100,
    BACKEND_INIT_FAILED = 101,
    DEVICE_NOT_FOUND = 102,
    DEVICE_BUSY = 103,
    KERNEL_LOAD_FAILED = 104,
    KERNEL_EXECUTION_FAILED = 105,
    GPU_OUT_OF_MEMORY = 106,
    BUFFER_CREATION_FAILED = 107,
    TEXTURE_CREATION_FAILED = 108,
    
    // SLANG compilation errors (200-299)
    SLANG_COMPILER_NOT_FOUND = 200,
    SLANG_COMPILATION_FAILED = 201,
    SLANG_SYNTAX_ERROR = 202,
    SLANG_LINK_ERROR = 203,
    BYTECODE_GENERATION_FAILED = 204,
    INTERMEDIATE_FILE_ERROR = 205,
    
    // Imaging errors (300-399)
    IMAGE_LOAD_FAILED = 300,
    IMAGE_SAVE_FAILED = 301,
    UNSUPPORTED_FORMAT = 302,
    COLOR_CONVERSION_FAILED = 303,
    IMAGE_RESIZE_FAILED = 304,
    CORRUPTED_IMAGE_DATA = 305,
    
    // System errors (400-499)
    SYSTEM_INTERROGATION_FAILED = 400,
    RUNTIME_DETECTION_FAILED = 401,
    DEVICE_ENUMERATION_FAILED = 402,
    VERSION_DETECTION_FAILED = 403,
    LIBRARY_LOAD_FAILED = 404,
    
    // Test errors (500-599)
    TEST_SETUP_FAILED = 500,
    TEST_EXECUTION_FAILED = 501,
    TEST_VALIDATION_FAILED = 502,
    TEST_TIMEOUT = 503,
    REFERENCE_DATA_MISSING = 504,
    STATISTICAL_ANALYSIS_FAILED = 505
};

/**
 * @brief Detailed error information with context
 */
struct ErrorInfo {
    ErrorCategory category;
    ErrorCode code;
    std::string message;
    std::string context;           ///< Additional context (file path, device name, etc.)
    std::string suggestion;        ///< Suggested resolution steps
    std::vector<ErrorInfo> nested; ///< Nested/chained errors
    
    ErrorInfo(ErrorCategory cat, ErrorCode err_code, const std::string& msg)
        : category(cat), code(err_code), message(msg) {}
    
    ErrorInfo(ErrorCategory cat, ErrorCode err_code, const std::string& msg, 
              const std::string& ctx)
        : category(cat), code(err_code), message(msg), context(ctx) {}
    
    ErrorInfo(ErrorCategory cat, ErrorCode err_code, const std::string& msg,
              const std::string& ctx, const std::string& sug)
        : category(cat), code(err_code), message(msg), context(ctx), suggestion(sug) {}
};

/**
 * @brief Custom exception class for Kerntopia operations
 */
class KerntopiaException : public std::exception {
public:
    explicit KerntopiaException(const ErrorInfo& error)
        : error_info_(error) {}
    
    KerntopiaException(ErrorCategory category, ErrorCode code, const std::string& message)
        : error_info_(category, code, message) {}
    
    KerntopiaException(ErrorCategory category, ErrorCode code, const std::string& message,
                       const std::string& context)
        : error_info_(category, code, message, context) {}
    
    const char* what() const noexcept override {
        return error_info_.message.c_str();
    }
    
    const ErrorInfo& GetErrorInfo() const { return error_info_; }
    
    /**
     * @brief Add nested error for error chaining
     * 
     * @param nested_error Error to nest
     */
    void AddNestedError(const ErrorInfo& nested_error) {
        error_info_.nested.push_back(nested_error);
    }

private:
    ErrorInfo error_info_;
};

/**
 * @brief Result type for operations that may fail
 * 
 * Similar to std::expected (C++23) but compatible with C++17
 * 
 * @tparam T Success value type
 */
template<typename T>
class Result {
public:
    // Success constructor
    Result(const T& value) : has_value_(true), value_(value) {}
    Result(T&& value) : has_value_(true), value_(std::move(value)) {}
    
    // Error constructor
    Result(const ErrorInfo& error) : has_value_(false), error_(error) {}
    Result(ErrorCategory category, ErrorCode code, const std::string& message)
        : has_value_(false), error_(category, code, message) {}
    
    // Static factory methods
    static Result<T> Success(const T& value) { return Result<T>(value); }
    static Result<T> Success(T&& value) { return Result<T>(std::move(value)); }
    static Result<T> Error(const ErrorInfo& error) { return Result<T>(error); }
    static Result<T> Error(ErrorCategory category, ErrorCode code, const std::string& message) {
        return Result<T>(category, code, message);
    }
    
    // Check if result contains value
    bool HasValue() const { return has_value_; }
    bool IsError() const { return !has_value_; }
    
    // Access value (throws if error)
    const T& GetValue() const {
        if (!has_value_) {
            throw KerntopiaException(error_);
        }
        return value_;
    }
    
    T& GetValue() {
        if (!has_value_) {
            throw KerntopiaException(error_);
        }
        return value_;
    }
    
    // Access error (undefined if success)
    const ErrorInfo& GetError() const { return error_; }
    
    // Convenience operators
    explicit operator bool() const { return has_value_; }
    const T& operator*() const { return GetValue(); }
    T& operator*() { return GetValue(); }
    const T* operator->() const { return &GetValue(); }
    T* operator->() { return &GetValue(); }

private:
    bool has_value_;
    T value_;
    ErrorInfo error_{ErrorCategory::GENERAL, ErrorCode::SUCCESS, ""};
};

/**
 * @brief Specialization for void operations
 */
template<>
class Result<void> {
public:
    // Success constructor
    Result() : has_value_(true) {}
    
    // Error constructor
    Result(const ErrorInfo& error) : has_value_(false), error_(error) {}
    Result(ErrorCategory category, ErrorCode code, const std::string& message)
        : has_value_(false), error_(category, code, message) {}
    
    // Static factory methods
    static Result<void> Success() { return Result<void>(); }
    static Result<void> Error(const ErrorInfo& error) { return Result<void>(error); }
    static Result<void> Error(ErrorCategory category, ErrorCode code, const std::string& message) {
        return Result<void>(category, code, message);
    }
    
    // Check if result is success
    bool HasValue() const { return has_value_; }
    bool IsError() const { return !has_value_; }
    
    // Access error (undefined if success)
    const ErrorInfo& GetError() const { return error_; }
    
    // Convenience operators
    explicit operator bool() const { return has_value_; }

private:
    bool has_value_;
    ErrorInfo error_{ErrorCategory::GENERAL, ErrorCode::SUCCESS, ""};
};

/**
 * @brief Error handling utilities
 */
class ErrorHandler {
public:
    /**
     * @brief Convert error code to human-readable string
     * 
     * @param code Error code
     * @return Descriptive string
     */
    static std::string ToString(ErrorCode code);
    
    /**
     * @brief Convert error category to string
     * 
     * @param category Error category
     * @return Category name
     */
    static std::string ToString(ErrorCategory category);
    
    /**
     * @brief Format error information for display
     * 
     * @param error Error information
     * @param include_nested Include nested errors in output
     * @return Formatted error message
     */
    static std::string FormatError(const ErrorInfo& error, bool include_nested = true);
    
    /**
     * @brief Get suggested resolution for error code
     * 
     * @param code Error code
     * @return Suggestion string
     */
    static std::string GetSuggestion(ErrorCode code);
    
    /**
     * @brief Log error information using the logging system
     * 
     * @param error Error information to log
     */
    static void LogError(const ErrorInfo& error);
};

// Convenience macros for error creation

#define KERNTOPIA_ERROR(category, code, message) \
    kerntopia::ErrorInfo(category, code, message)

#define KERNTOPIA_ERROR_CTX(category, code, message, context) \
    kerntopia::ErrorInfo(category, code, message, context)

#define KERNTOPIA_ERROR_FULL(category, code, message, context, suggestion) \
    kerntopia::ErrorInfo(category, code, message, context, suggestion)

// Result creation macros
#define KERNTOPIA_SUCCESS(value) \
    kerntopia::Result<decltype(value)>::Success(value)

#define KERNTOPIA_VOID_SUCCESS() \
    kerntopia::Result<void>::Success()

#define KERNTOPIA_RESULT_ERROR(type, category, code, message) \
    kerntopia::Result<type>::Error(category, code, message)

// Error checking macros
#define KERNTOPIA_TRY(result) \
    do { \
        if (!(result)) { \
            return kerntopia::Result<decltype((result).GetValue())>::Error((result).GetError()); \
        } \
    } while(0)

#define KERNTOPIA_TRY_VOID(result) \
    do { \
        if (!(result)) { \
            return kerntopia::Result<void>::Error((result).GetError()); \
        } \
    } while(0)

} // namespace kerntopia