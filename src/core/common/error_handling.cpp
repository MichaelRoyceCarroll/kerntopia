#include "error_handling.hpp"
#include "logger.hpp"

#include <sstream>

namespace kerntopia {

std::string ErrorHandler::ToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::SUCCESS:
            return "Success";
            
        // General errors
        case ErrorCode::UNKNOWN_ERROR:
            return "Unknown error occurred";
        case ErrorCode::INVALID_ARGUMENT:
            return "Invalid argument provided";
        case ErrorCode::OUT_OF_MEMORY:
            return "Out of memory";
        case ErrorCode::FILE_NOT_FOUND:
            return "File not found";
        case ErrorCode::PERMISSION_DENIED:
            return "Permission denied";
        case ErrorCode::OPERATION_CANCELLED:
            return "Operation cancelled";
            
        // Backend errors
        case ErrorCode::BACKEND_NOT_AVAILABLE:
            return "GPU backend not available";
        case ErrorCode::BACKEND_INIT_FAILED:
            return "Backend initialization failed";
        case ErrorCode::DEVICE_NOT_FOUND:
            return "GPU device not found";
        case ErrorCode::DEVICE_BUSY:
            return "GPU device is busy";
        case ErrorCode::KERNEL_LOAD_FAILED:
            return "Failed to load kernel";
        case ErrorCode::KERNEL_EXECUTION_FAILED:
            return "Kernel execution failed";
        case ErrorCode::GPU_OUT_OF_MEMORY:
            return "GPU out of memory";
        case ErrorCode::BUFFER_CREATION_FAILED:
            return "Failed to create GPU buffer";
        case ErrorCode::TEXTURE_CREATION_FAILED:
            return "Failed to create GPU texture";
            
        // SLANG compilation errors
        case ErrorCode::SLANG_COMPILER_NOT_FOUND:
            return "SLANG compiler not found";
        case ErrorCode::SLANG_COMPILATION_FAILED:
            return "SLANG compilation failed";
        case ErrorCode::SLANG_SYNTAX_ERROR:
            return "SLANG syntax error";
        case ErrorCode::SLANG_LINK_ERROR:
            return "SLANG linking error";
        case ErrorCode::BYTECODE_GENERATION_FAILED:
            return "Bytecode generation failed";
        case ErrorCode::INTERMEDIATE_FILE_ERROR:
            return "Intermediate file error";
            
        // Imaging errors
        case ErrorCode::IMAGE_LOAD_FAILED:
            return "Failed to load image";
        case ErrorCode::IMAGE_SAVE_FAILED:
            return "Failed to save image";
        case ErrorCode::UNSUPPORTED_FORMAT:
            return "Unsupported image format";
        case ErrorCode::COLOR_CONVERSION_FAILED:
            return "Color space conversion failed";
        case ErrorCode::IMAGE_RESIZE_FAILED:
            return "Image resize failed";
        case ErrorCode::CORRUPTED_IMAGE_DATA:
            return "Corrupted image data";
            
        // System errors
        case ErrorCode::SYSTEM_INTERROGATION_FAILED:
            return "System interrogation failed";
        case ErrorCode::RUNTIME_DETECTION_FAILED:
            return "Runtime detection failed";
        case ErrorCode::DEVICE_ENUMERATION_FAILED:
            return "Device enumeration failed";
        case ErrorCode::VERSION_DETECTION_FAILED:
            return "Version detection failed";
        case ErrorCode::LIBRARY_LOAD_FAILED:
            return "Library loading failed";
            
        // Test errors
        case ErrorCode::TEST_SETUP_FAILED:
            return "Test setup failed";
        case ErrorCode::TEST_EXECUTION_FAILED:
            return "Test execution failed";
        case ErrorCode::TEST_VALIDATION_FAILED:
            return "Test validation failed";
        case ErrorCode::TEST_TIMEOUT:
            return "Test execution timeout";
        case ErrorCode::REFERENCE_DATA_MISSING:
            return "Reference data missing";
        case ErrorCode::STATISTICAL_ANALYSIS_FAILED:
            return "Statistical analysis failed";
            
        default:
            return "Unknown error code";
    }
}

std::string ErrorHandler::ToString(ErrorCategory category) {
    switch (category) {
        case ErrorCategory::GENERAL:
            return "General";
        case ErrorCategory::BACKEND:
            return "Backend";
        case ErrorCategory::SLANG_COMPILE:
            return "SLANG Compilation";
        case ErrorCategory::IMAGING:
            return "Imaging";
        case ErrorCategory::SYSTEM:
            return "System";
        case ErrorCategory::TEST:
            return "Test";
        case ErrorCategory::VALIDATION:
            return "Validation";
        default:
            return "Unknown";
    }
}

std::string ErrorHandler::FormatError(const ErrorInfo& error, bool include_nested) {
    std::ostringstream oss;
    
    // Main error
    oss << "[" << ToString(error.category) << "] " 
        << ToString(error.code);
    
    if (!error.message.empty()) {
        oss << ": " << error.message;
    }
    
    if (!error.context.empty()) {
        oss << " (Context: " << error.context << ")";
    }
    
    if (!error.suggestion.empty()) {
        oss << " | Suggestion: " << error.suggestion;
    }
    
    // Nested errors
    if (include_nested && !error.nested.empty()) {
        oss << "\nNested errors:";
        for (size_t i = 0; i < error.nested.size(); ++i) {
            oss << "\n  " << (i + 1) << ". " << FormatError(error.nested[i], false);
        }
    }
    
    return oss.str();
}

std::string ErrorHandler::GetSuggestion(ErrorCode code) {
    switch (code) {
        case ErrorCode::BACKEND_NOT_AVAILABLE:
            return "Check if GPU drivers are installed and up to date";
        case ErrorCode::SLANG_COMPILER_NOT_FOUND:
            return "Ensure SLANG compiler is in PATH or SLANG_PATH environment variable is set";
        case ErrorCode::FILE_NOT_FOUND:
            return "Verify file path is correct and file exists";
        case ErrorCode::PERMISSION_DENIED:
            return "Check file/directory permissions or run with appropriate privileges";
        case ErrorCode::GPU_OUT_OF_MEMORY:
            return "Try reducing image size or buffer allocation, or close other GPU applications";
        case ErrorCode::DEVICE_NOT_FOUND:
            return "Check GPU device index and ensure device is not in use by other processes";
        case ErrorCode::UNSUPPORTED_FORMAT:
            return "Convert image to supported format (PNG, JPG, EXR) or check format specifications";
        case ErrorCode::TEST_TIMEOUT:
            return "Increase timeout value or check for infinite loops in kernel code";
        case ErrorCode::LIBRARY_LOAD_FAILED:
            return "Ensure required runtime libraries (CUDA, Vulkan) are installed";
        default:
            return "Check logs for more detailed information";
    }
}

void ErrorHandler::LogError(const ErrorInfo& error) {
    // Determine log level based on error severity
    LogLevel level = LogLevel::ERROR;
    
    // Map category to component for logging
    LogComponent component = LogComponent::GENERAL;
    switch (error.category) {
        case ErrorCategory::BACKEND:
            component = LogComponent::BACKEND;
            break;
        case ErrorCategory::SLANG_COMPILE:
            component = LogComponent::SLANG;
            break;
        case ErrorCategory::IMAGING:
            component = LogComponent::IMAGING;
            break;
        case ErrorCategory::SYSTEM:
            component = LogComponent::SYSTEM;
            break;
        case ErrorCategory::TEST:
            component = LogComponent::TEST;
            break;
        case ErrorCategory::VALIDATION:
        case ErrorCategory::GENERAL:
        default:
            component = LogComponent::GENERAL;
            break;
    }
    
    // Log the formatted error
    Logger::GetInstance().Log(level, component, FormatError(error, true));
}

} // namespace kerntopia