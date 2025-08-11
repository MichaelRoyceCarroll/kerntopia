#include "logger.hpp"

#include <iostream>
#include <filesystem>

namespace kerntopia {

std::unique_ptr<Logger> Logger::instance_ = nullptr;
std::mutex Logger::instance_mutex_;

bool Logger::Initialize(const Config& config) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    if (instance_) {
        return false; // Already initialized
    }
    
    instance_ = std::unique_ptr<Logger>(new Logger());
    instance_->config_ = config;
    
    // Initialize file output if requested
    if (config.log_to_file && !config.log_file_path.empty()) {
        instance_->log_file_ = std::make_unique<std::ofstream>(
            config.log_file_path, 
            std::ios::out | std::ios::app
        );
        
        if (!instance_->log_file_->is_open()) {
            std::cerr << "Warning: Failed to open log file: " << config.log_file_path << std::endl;
            instance_->config_.log_to_file = false;
        } else {
            // Get current file size
            std::filesystem::path log_path(config.log_file_path);
            if (std::filesystem::exists(log_path)) {
                instance_->current_file_size_ = std::filesystem::file_size(log_path);
            }
        }
    }
    
    // Log initialization
    instance_->Log(LogLevel::INFO, LogComponent::GENERAL, "Kerntopia logger initialized");
    
    return true;
}

Logger& Logger::GetInstance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) {
        // Create default instance if not initialized
        Config default_config;
        instance_ = std::unique_ptr<Logger>(new Logger());
        instance_->config_ = default_config;
    }
    return *instance_;
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (instance_) {
        instance_->Log(LogLevel::INFO, LogComponent::GENERAL, "Kerntopia logger shutting down");
        instance_->log_file_.reset();
        instance_.reset();
    }
}

void Logger::Log(LogLevel level, LogComponent component, const std::string& message) {
    // Check if we should log this level
    if (level < config_.min_level) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    std::string formatted_message = FormatMessage(level, component, message);
    WriteToOutputs(formatted_message);
    
    // Rotate log file if needed
    if (config_.log_to_file && log_file_) {
        RotateLogFileIfNeeded();
    }
}

void Logger::SetLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    config_.min_level = level;
}

void Logger::SetConsoleOutput(bool enabled) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    config_.log_to_console = enabled;
}

void Logger::SetFileOutput(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    // Close existing file
    if (log_file_) {
        log_file_->close();
        log_file_.reset();
    }
    
    config_.log_file_path = file_path;
    config_.log_to_file = !file_path.empty();
    
    if (config_.log_to_file) {
        log_file_ = std::make_unique<std::ofstream>(
            file_path, 
            std::ios::out | std::ios::app
        );
        
        if (!log_file_->is_open()) {
            std::cerr << "Warning: Failed to open log file: " << file_path << std::endl;
            config_.log_to_file = false;
            log_file_.reset();
        } else {
            // Get current file size
            std::filesystem::path log_path(file_path);
            if (std::filesystem::exists(log_path)) {
                current_file_size_ = std::filesystem::file_size(log_path);
            } else {
                current_file_size_ = 0;
            }
        }
    }
}

void Logger::WriteToOutputs(const std::string& formatted_message) {
    // Console output
    if (config_.log_to_console) {
        std::cout << formatted_message << std::endl;
    }
    
    // File output
    if (config_.log_to_file && log_file_) {
        *log_file_ << formatted_message << std::endl;
        log_file_->flush();
        current_file_size_ += formatted_message.length() + 1; // +1 for newline
    }
}

std::string Logger::FormatMessage(LogLevel level, LogComponent component, const std::string& message) {
    std::ostringstream oss;
    
    // Timestamp
    if (config_.include_timestamps) {
        oss << "[" << GetTimestamp() << "] ";
    }
    
    // Log level
    oss << "[" << GetLevelString(level) << "] ";
    
    // Component
    if (config_.include_component) {
        oss << "[" << GetComponentString(component) << "] ";
    }
    
    // Thread ID
    if (config_.include_thread_id) {
        oss << "[Thread:" << std::this_thread::get_id() << "] ";
    }
    
    // Message
    oss << message;
    
    return oss.str();
}

std::string Logger::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

const char* Logger::GetLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:    return "DEBUG";
        case LogLevel::INFO:     return "INFO ";
        case LogLevel::WARNING:  return "WARN ";
        case LogLevel::ERROR:    return "ERROR";
        case LogLevel::CRITICAL: return "CRIT ";
        default:                 return "UNKN ";
    }
}

const char* Logger::GetComponentString(LogComponent component) {
    switch (component) {
        case LogComponent::GENERAL:     return "GEN ";
        case LogComponent::BACKEND:     return "BACK";
        case LogComponent::SLANG:       return "SLNG";
        case LogComponent::IMAGING:     return "IMG ";
        case LogComponent::SYSTEM:      return "SYS ";
        case LogComponent::TEST:        return "TEST";
        case LogComponent::PERFORMANCE: return "PERF";
        default:                        return "UNK ";
    }
}

void Logger::RotateLogFileIfNeeded() {
    if (!log_file_ || current_file_size_ < config_.max_file_size_mb * 1024 * 1024) {
        return;
    }
    
    // Close current file
    log_file_->close();
    
    // Rename current file with timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream backup_name;
    backup_name << config_.log_file_path << "." 
                << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    
    try {
        std::filesystem::rename(config_.log_file_path, backup_name.str());
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Warning: Failed to rotate log file: " << e.what() << std::endl;
    }
    
    // Reopen log file
    log_file_ = std::make_unique<std::ofstream>(
        config_.log_file_path, 
        std::ios::out | std::ios::trunc
    );
    
    current_file_size_ = 0;
    
    if (log_file_->is_open()) {
        Log(LogLevel::INFO, LogComponent::GENERAL, "Log file rotated");
    }
}

} // namespace kerntopia