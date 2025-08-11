#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <memory>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>

namespace kerntopia {

/**
 * @brief Log levels for categorizing messages
 */
enum class LogLevel {
    DEBUG = 0,    ///< Detailed information for debugging
    INFO = 1,     ///< General information messages
    WARNING = 2,  ///< Warning messages for potential issues
    ERROR = 3,    ///< Error messages for failures
    CRITICAL = 4  ///< Critical errors that may cause termination
};

/**
 * @brief Component tags for categorizing log messages by system area
 */
enum class LogComponent {
    GENERAL,      ///< General application messages
    BACKEND,      ///< Backend abstraction and GPU operations
    SLANG,        ///< SLANG compilation and kernel management
    IMAGING,      ///< Image processing and I/O operations
    SYSTEM,       ///< System interrogation and device detection
    TEST,         ///< Test framework and execution
    PERFORMANCE   ///< Performance measurement and analysis
};

/**
 * @brief Thread-safe logging system with structured output and file rotation
 * 
 * Provides comprehensive logging capabilities for the Kerntopia suite including:
 * - Multiple log levels with filtering
 * - Component-based categorization
 * - Thread-safe operation
 * - Timestamped messages
 * - File and console output
 * - Educational formatting for debugging
 */
class Logger {
public:
    /**
     * @brief Configuration for logger behavior
     */
    struct Config {
        LogLevel min_level = LogLevel::INFO;      ///< Minimum level to log
        bool log_to_console = true;               ///< Enable console output
        bool log_to_file = false;                 ///< Enable file output
        std::string log_file_path = "";           ///< Path to log file
        bool include_timestamps = true;           ///< Include timestamps in messages
        bool include_thread_id = false;           ///< Include thread IDs
        bool include_component = true;            ///< Include component tags
        size_t max_file_size_mb = 10;            ///< Max log file size before rotation
    };

    /**
     * @brief Initialize global logger with configuration
     * 
     * @param config Logger configuration
     * @return True if initialization successful
     */
    static bool Initialize(const Config& config);

    /**
     * @brief Get global logger instance
     * 
     * @return Reference to global logger
     */
    static Logger& GetInstance();

    /**
     * @brief Shutdown logger and cleanup resources
     */
    static void Shutdown();

    /**
     * @brief Log a message with specified level and component
     * 
     * @param level Log level
     * @param component Component category
     * @param message Message to log
     */
    void Log(LogLevel level, LogComponent component, const std::string& message);

    /**
     * @brief Log formatted message with printf-style formatting
     * 
     * @param level Log level
     * @param component Component category
     * @param format Printf-style format string
     * @param args Format arguments
     */
    template<typename... Args>
    void LogFormat(LogLevel level, LogComponent component, const char* format, Args&&... args) {
        char buffer[2048];
        std::snprintf(buffer, sizeof(buffer), format, std::forward<Args>(args)...);
        Log(level, component, std::string(buffer));
    }

    /**
     * @brief Set minimum log level for filtering
     * 
     * @param level Minimum level to log
     */
    void SetLogLevel(LogLevel level);

    /**
     * @brief Enable or disable console output
     * 
     * @param enabled True to enable console output
     */
    void SetConsoleOutput(bool enabled);

    /**
     * @brief Enable file output with specified path
     * 
     * @param file_path Path to log file (empty to disable)
     */
    void SetFileOutput(const std::string& file_path);

public:
    ~Logger() = default;
    
private:
    Logger() = default;

    // Non-copyable
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void WriteToOutputs(const std::string& formatted_message);
    std::string FormatMessage(LogLevel level, LogComponent component, const std::string& message);
    std::string GetTimestamp();
    const char* GetLevelString(LogLevel level);
    const char* GetComponentString(LogComponent component);
    void RotateLogFileIfNeeded();

    Config config_;
    std::unique_ptr<std::ofstream> log_file_;
    std::mutex log_mutex_;
    size_t current_file_size_ = 0;
    
    static std::unique_ptr<Logger> instance_;
    static std::mutex instance_mutex_;
};

// Convenience macros for logging

#define KERNTOPIA_LOG_DEBUG(component, message) \
    kerntopia::Logger::GetInstance().Log(kerntopia::LogLevel::DEBUG, component, message)

#define KERNTOPIA_LOG_INFO(component, message) \
    kerntopia::Logger::GetInstance().Log(kerntopia::LogLevel::INFO, component, message)

#define KERNTOPIA_LOG_WARNING(component, message) \
    kerntopia::Logger::GetInstance().Log(kerntopia::LogLevel::WARNING, component, message)

#define KERNTOPIA_LOG_ERROR(component, message) \
    kerntopia::Logger::GetInstance().Log(kerntopia::LogLevel::ERROR, component, message)

#define KERNTOPIA_LOG_CRITICAL(component, message) \
    kerntopia::Logger::GetInstance().Log(kerntopia::LogLevel::CRITICAL, component, message)

// Formatted logging macros
#define KERNTOPIA_LOG_DEBUG_FMT(component, format, ...) \
    kerntopia::Logger::GetInstance().LogFormat(kerntopia::LogLevel::DEBUG, component, format, __VA_ARGS__)

#define KERNTOPIA_LOG_INFO_FMT(component, format, ...) \
    kerntopia::Logger::GetInstance().LogFormat(kerntopia::LogLevel::INFO, component, format, __VA_ARGS__)

#define KERNTOPIA_LOG_WARNING_FMT(component, format, ...) \
    kerntopia::Logger::GetInstance().LogFormat(kerntopia::LogLevel::WARNING, component, format, __VA_ARGS__)

#define KERNTOPIA_LOG_ERROR_FMT(component, format, ...) \
    kerntopia::Logger::GetInstance().LogFormat(kerntopia::LogLevel::ERROR, component, format, __VA_ARGS__)

#define KERNTOPIA_LOG_CRITICAL_FMT(component, format, ...) \
    kerntopia::Logger::GetInstance().LogFormat(kerntopia::LogLevel::CRITICAL, component, format, __VA_ARGS__)

// Component-specific convenience macros
#define LOG_BACKEND_INFO(message) KERNTOPIA_LOG_INFO(kerntopia::LogComponent::BACKEND, message)
#define LOG_BACKEND_ERROR(message) KERNTOPIA_LOG_ERROR(kerntopia::LogComponent::BACKEND, message)
#define LOG_BACKEND_DEBUG(message) KERNTOPIA_LOG_DEBUG(kerntopia::LogComponent::BACKEND, message)

#define LOG_SLANG_INFO(message) KERNTOPIA_LOG_INFO(kerntopia::LogComponent::SLANG, message)
#define LOG_SLANG_ERROR(message) KERNTOPIA_LOG_ERROR(kerntopia::LogComponent::SLANG, message)
#define LOG_SLANG_DEBUG(message) KERNTOPIA_LOG_DEBUG(kerntopia::LogComponent::SLANG, message)

#define LOG_SYSTEM_INFO(message) KERNTOPIA_LOG_INFO(kerntopia::LogComponent::SYSTEM, message)
#define LOG_SYSTEM_ERROR(message) KERNTOPIA_LOG_ERROR(kerntopia::LogComponent::SYSTEM, message)
#define LOG_SYSTEM_DEBUG(message) KERNTOPIA_LOG_DEBUG(kerntopia::LogComponent::SYSTEM, message)

#define LOG_TEST_INFO(message) KERNTOPIA_LOG_INFO(kerntopia::LogComponent::TEST, message)
#define LOG_TEST_ERROR(message) KERNTOPIA_LOG_ERROR(kerntopia::LogComponent::TEST, message)
#define LOG_TEST_DEBUG(message) KERNTOPIA_LOG_DEBUG(kerntopia::LogComponent::TEST, message)

#define LOG_PERF_INFO(message) KERNTOPIA_LOG_INFO(kerntopia::LogComponent::PERFORMANCE, message)
#define LOG_PERF_DEBUG(message) KERNTOPIA_LOG_DEBUG(kerntopia::LogComponent::PERFORMANCE, message)

} // namespace kerntopia