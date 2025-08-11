#include "runtime_loader.hpp"
#include "../common/logger.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>

#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
#else
    #include <dlfcn.h>
    #include <unistd.h>
    #include <sys/stat.h>
#endif

namespace kerntopia {

RuntimeLoader::RuntimeLoader() {
    LOG_SYSTEM_DEBUG("RuntimeLoader initialized");
}

RuntimeLoader::~RuntimeLoader() {
    // Unload all libraries
    for (auto& [path, handle] : loaded_libraries_) {
        UnloadLibrary(handle);
    }
    LOG_SYSTEM_DEBUG("RuntimeLoader destroyed");
}

Result<std::map<std::string, LibraryInfo>> RuntimeLoader::ScanForLibraries(const std::vector<std::string>& patterns) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::map<std::string, LibraryInfo> found_libraries;
    
    // Get system search paths
    auto paths_result = GetSystemPaths();
    if (!paths_result) {
        return Result<std::map<std::string, LibraryInfo>>::Error(ErrorCategory::SYSTEM, ErrorCode::SYSTEM_INTERROGATION_FAILED, "Failed to get system paths");
    }
    
    search_paths_ = *paths_result;
    
    // Search each path
    for (const std::string& path : search_paths_) {
        auto scan_result = ScanDirectory(path, patterns);
        if (scan_result) {
            for (const std::string& library_path : *scan_result) {
                auto metadata_result = GetFileMetadata(library_path);
                if (metadata_result) {
                    LibraryInfo info = *metadata_result;
                    
                    // Check for duplicates
                    auto existing = found_libraries.find(info.name);
                    if (existing != found_libraries.end()) {
                        existing->second.is_primary = false;
                        existing->second.duplicate_paths.push_back(library_path);
                        info.is_primary = true; // Latest found becomes primary
                    }
                    
                    found_libraries[info.name] = info;
                }
            }
        }
    }
    
    Logger::GetInstance().LogFormat(LogLevel::INFO, LogComponent::SYSTEM, "Found %zu libraries matching patterns", found_libraries.size());
    return Result<std::map<std::string, LibraryInfo>>::Success(found_libraries);
}

Result<LibraryInfo> RuntimeLoader::FindLibrary(const std::string& library_name) {
    std::vector<std::string> patterns = {library_name};
    auto scan_result = ScanForLibraries(patterns);
    if (!scan_result) {
        return Result<LibraryInfo>::Error(ErrorCategory::SYSTEM, ErrorCode::LIBRARY_LOAD_FAILED, "Library scan failed: " + library_name);
    }
    
    auto& libraries = *scan_result;
    auto it = libraries.find(library_name);
    if (it == libraries.end()) {
        return Result<LibraryInfo>::Error(ErrorCategory::SYSTEM, ErrorCode::FILE_NOT_FOUND, "Library not found: " + library_name);
    }
    
    return Result<LibraryInfo>::Success(it->second);
}

std::vector<std::string> RuntimeLoader::GetSearchPaths() const {
    return search_paths_;
}

Result<LibraryHandle> RuntimeLoader::LoadLibrary(const std::string& library_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if already loaded
    auto it = loaded_libraries_.find(library_path);
    if (it != loaded_libraries_.end()) {
        return KERNTOPIA_SUCCESS(it->second);
    }
    
    // Platform-specific loading
    LibraryHandle handle = nullptr;
    
#ifdef _WIN32
    handle = LoadLibraryExA(library_path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#else
    handle = dlopen(library_path.c_str(), RTLD_LAZY | RTLD_LOCAL);
#endif
    
    if (!handle) {
        std::string error_msg = GetLastErrorString();
        return KERNTOPIA_RESULT_ERROR(LibraryHandle, ErrorCategory::SYSTEM,
                                     ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to load library: " + library_path + " - " + error_msg);
    }
    
    loaded_libraries_[library_path] = handle;
    handle_to_path_[handle] = library_path;
    
    LOG_SYSTEM_INFO("Loaded library: " + library_path);
    return KERNTOPIA_SUCCESS(handle);
}

Result<void> RuntimeLoader::UnloadLibrary(LibraryHandle handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto path_it = handle_to_path_.find(handle);
    if (path_it == handle_to_path_.end()) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::SYSTEM,
                                     ErrorCode::INVALID_ARGUMENT,
                                     "Invalid library handle");
    }
    
    std::string path = path_it->second;
    
#ifdef _WIN32
    if (!FreeLibrary(handle)) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::SYSTEM,
                                     ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to unload library: " + path);
    }
#else
    if (dlclose(handle) != 0) {
        return KERNTOPIA_RESULT_ERROR(void, ErrorCategory::SYSTEM,
                                     ErrorCode::LIBRARY_LOAD_FAILED,
                                     "Failed to unload library: " + path);
    }
#endif
    
    loaded_libraries_.erase(path);
    handle_to_path_.erase(handle);
    
    LOG_SYSTEM_INFO("Unloaded library: " + path);
    return KERNTOPIA_VOID_SUCCESS();
}

void* RuntimeLoader::GetSymbol(LibraryHandle handle, const std::string& symbol_name) {
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(handle, symbol_name.c_str()));
#else
    return dlsym(handle, symbol_name.c_str());
#endif
}

bool RuntimeLoader::HasSymbol(LibraryHandle handle, const std::string& symbol_name) {
    return GetSymbol(handle, symbol_name) != nullptr;
}

std::string RuntimeLoader::GetLibraryExtension() {
#ifdef _WIN32
    return ".dll";
#else
    return ".so";
#endif
}

std::string RuntimeLoader::GetLibraryPrefix() {
#ifdef _WIN32
    return "";
#else
    return "lib";
#endif
}

std::string RuntimeLoader::BuildLibraryFilename(const std::string& base_name) {
    return GetLibraryPrefix() + base_name + GetLibraryExtension();
}

std::string RuntimeLoader::GetLastErrorString() {
#ifdef _WIN32
    DWORD error = GetLastError();
    char* msg_buf = nullptr;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&msg_buf), 0, nullptr);
    
    std::string result = msg_buf ? msg_buf : "Unknown error";
    if (msg_buf) LocalFree(msg_buf);
    return result;
#else
    const char* error = dlerror();
    return error ? error : "Unknown error";
#endif
}

// Private implementation methods (simplified for now)
Result<std::vector<std::string>> RuntimeLoader::GetSystemPaths() const {
    std::vector<std::string> paths;
    
#ifdef _WIN32
    // Windows system paths
    char system_dir[MAX_PATH];
    if (GetSystemDirectoryA(system_dir, MAX_PATH)) {
        paths.push_back(system_dir);
    }
    
    // PATH environment variable
    char* path_env = nullptr;
    size_t path_len = 0;
    if (_dupenv_s(&path_env, &path_len, "PATH") == 0 && path_env) {
        std::string path_str(path_env);
        free(path_env);
        
        size_t pos = 0;
        while (pos < path_str.length()) {
            size_t next = path_str.find(';', pos);
            if (next == std::string::npos) {
                paths.push_back(path_str.substr(pos));
                break;
            }
            paths.push_back(path_str.substr(pos, next - pos));
            pos = next + 1;
        }
    }
#else
    // Linux system paths
    paths.push_back("/usr/lib");
    paths.push_back("/usr/lib64");
    paths.push_back("/usr/local/lib");
    paths.push_back("/lib");
    paths.push_back("/lib64");
    
    // LD_LIBRARY_PATH
    const char* ld_path = getenv("LD_LIBRARY_PATH");
    if (ld_path) {
        std::string path_str(ld_path);
        size_t pos = 0;
        while (pos < path_str.length()) {
            size_t next = path_str.find(':', pos);
            if (next == std::string::npos) {
                paths.push_back(path_str.substr(pos));
                break;
            }
            paths.push_back(path_str.substr(pos, next - pos));
            pos = next + 1;
        }
    }
#endif
    
    return KERNTOPIA_SUCCESS(paths);
}

Result<std::vector<std::string>> RuntimeLoader::ScanDirectory(const std::string& directory, 
                                                             const std::vector<std::string>& patterns) const {
    std::vector<std::string> found_files;
    
    try {
        if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
            return KERNTOPIA_SUCCESS(found_files); // Empty result, not an error
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (!entry.is_regular_file()) continue;
            
            std::string filename = entry.path().filename().string();
            
            // Check if filename matches any pattern
            for (const std::string& pattern : patterns) {
                if (filename.find(pattern) != std::string::npos) {
                    found_files.push_back(entry.path().string());
                    break;
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        // Not a critical error, just log and continue
        LOG_SYSTEM_DEBUG("Error scanning directory " + directory + ": " + e.what());
    }
    
    return KERNTOPIA_SUCCESS(found_files);
}

Result<LibraryInfo> RuntimeLoader::GetFileMetadata(const std::string& file_path) const {
    LibraryInfo info;
    info.full_path = file_path;
    info.name = std::filesystem::path(file_path).stem().string();
    
    try {
        if (std::filesystem::exists(file_path)) {
            info.file_size = std::filesystem::file_size(file_path);
            
            auto write_time = std::filesystem::last_write_time(file_path);
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                write_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
            auto time_t = std::chrono::system_clock::to_time_t(sctp);
            
            std::ostringstream oss;
            oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            info.last_modified = oss.str();
        }
    } catch (const std::filesystem::filesystem_error& e) {
        return KERNTOPIA_RESULT_ERROR(LibraryInfo, ErrorCategory::SYSTEM,
                                     ErrorCode::FILE_NOT_FOUND,
                                     "Failed to get file metadata: " + std::string(e.what()));
    }
    
    return KERNTOPIA_SUCCESS(info);
}

} // namespace kerntopia