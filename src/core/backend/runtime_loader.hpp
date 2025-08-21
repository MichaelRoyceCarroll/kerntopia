#pragma once

#include "../common/error_handling.hpp"

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <mutex>

#ifdef _WIN32
    #include <windows.h>
    using LibraryHandle = HMODULE;
#else
    using LibraryHandle = void*;
#endif

namespace kerntopia {

/**
 * @brief Information about a detected runtime library
 */
struct LibraryInfo {
    std::string name;                           ///< Library name (e.g., "cudart64_12")
    std::string full_path;                      ///< Full path to library file
    std::string version;                        ///< Library version if detectable
    std::string checksum;                       ///< File checksum for verification
    uint64_t file_size = 0;                     ///< File size in bytes
    std::string last_modified;                  ///< Last modification timestamp
    bool is_primary = true;                     ///< Primary library if duplicates found
    std::vector<std::string> duplicate_paths;   ///< Other paths where library was found
};

/**
 * @brief Cross-platform dynamic library loading and management
 * 
 * Provides comprehensive dynamic library detection, loading, and symbol resolution
 * with detailed metadata collection for audit trails and debugging.
 * 
 * Key features:
 * - Cross-platform dynamic library loading (Windows/Linux)
 * - Comprehensive PATH scanning with duplicate detection
 * - Version detection and metadata collection
 * - Safe symbol resolution with type checking
 * - Complete audit trail for all operations
 */
class RuntimeLoader {
private:
    // Private constructor to prevent direct instantiation
    RuntimeLoader();
    
    // Delete copy constructor and assignment operator to prevent copying
    RuntimeLoader(const RuntimeLoader&) = delete;
    RuntimeLoader& operator=(const RuntimeLoader&) = delete;

public:
    // Public static method to get the single instance of RuntimeLoader
    static RuntimeLoader& GetInstance();
    
    ~RuntimeLoader();
    
    // Library detection and scanning
    
    /**
     * @brief Scan system PATH for libraries matching pattern
     * 
     * @param patterns Library name patterns to search (e.g., {"cudart", "nvcuda"})
     * @return Map of library name to information
     */
    Result<std::map<std::string, LibraryInfo>> ScanForLibraries(const std::vector<std::string>& patterns);
    
    /**
     * @brief Find specific library in system
     * 
     * @param library_name Library name to find
     * @return Library information or error if not found
     */
    Result<LibraryInfo> FindLibrary(const std::string& library_name);
    
    /**
     * @brief Get all search paths used for library detection
     * 
     * @return Vector of search paths in order
     */
    std::vector<std::string> GetSearchPaths() const;
    
    // Library loading and management
    
    /**
     * @brief Load library from path
     * 
     * @param library_path Full path to library file
     * @return Library handle or error
     */
    Result<LibraryHandle> LoadLibrary(const std::string& library_path);
    
    /**
     * @brief Unload previously loaded library
     * 
     * @param handle Library handle from LoadLibrary
     * @return Success result
     */
    Result<void> UnloadLibrary(LibraryHandle handle);
    
    /**
     * @brief Check if library is currently loaded
     * 
     * @param library_path Library path
     * @return True if library is loaded
     */
    bool IsLibraryLoaded(const std::string& library_path) const;
    
    // Symbol resolution
    
    /**
     * @brief Get function pointer from loaded library
     * 
     * @param handle Library handle
     * @param symbol_name Function symbol name
     * @return Function pointer or nullptr if not found
     */
    void* GetSymbol(LibraryHandle handle, const std::string& symbol_name);
    
    /**
     * @brief Get typed function pointer with error checking
     * 
     * @tparam FuncType Function pointer type
     * @param handle Library handle
     * @param symbol_name Function symbol name
     * @return Typed function pointer or error
     */
    template<typename FuncType>
    Result<FuncType> GetTypedSymbol(LibraryHandle handle, const std::string& symbol_name) {
        void* symbol = GetSymbol(handle, symbol_name);
        if (!symbol) {
            return KERNTOPIA_RESULT_ERROR(FuncType, ErrorCategory::SYSTEM, 
                                         ErrorCode::LIBRARY_LOAD_FAILED,
                                         "Symbol not found: " + symbol_name);
        }
        return KERNTOPIA_SUCCESS(reinterpret_cast<FuncType>(symbol));
    }
    
    /**
     * @brief Check if symbol exists in library
     * 
     * @param handle Library handle
     * @param symbol_name Symbol name to check
     * @return True if symbol exists
     */
    bool HasSymbol(LibraryHandle handle, const std::string& symbol_name);
    
    // Library metadata and validation
    
    /**
     * @brief Get version information from library
     * 
     * @param library_path Path to library file
     * @return Version string or error
     */
    Result<std::string> GetLibraryVersion(const std::string& library_path);
    
    /**
     * @brief Calculate file checksum for verification
     * 
     * @param file_path Path to file
     * @return SHA-256 checksum string or error
     */
    Result<std::string> CalculateChecksum(const std::string& file_path);
    
    /**
     * @brief Get file metadata (size, timestamps, etc.)
     * 
     * @param file_path Path to file
     * @return File metadata or error
     */
    Result<LibraryInfo> GetFileMetadata(const std::string& file_path) const;
    
    /**
     * @brief Validate library file integrity
     * 
     * @param library_path Path to library
     * @return Validation result
     */
    Result<void> ValidateLibrary(const std::string& library_path);
    
    // Utility functions
    
    /**
     * @brief Get platform-specific library extension
     * 
     * @return Library extension (".dll" on Windows, ".so" on Linux)
     */
    static std::string GetLibraryExtension();
    
    /**
     * @brief Get platform-specific library prefix
     * 
     * @return Library prefix ("" on Windows, "lib" on Linux)
     */
    static std::string GetLibraryPrefix();
    
    /**
     * @brief Build platform-specific library filename
     * 
     * @param base_name Base library name
     * @return Full library filename
     */
    static std::string BuildLibraryFilename(const std::string& base_name);
    
    /**
     * @brief Get last system error as string
     * 
     * @return Error description
     */
    static std::string GetLastErrorString();
    
    // Debug and diagnostics
    
    /**
     * @brief Get comprehensive diagnostic information
     * 
     * @return Debug information string
     */
    std::string GetDiagnostics() const;
    
    /**
     * @brief List all currently loaded libraries
     * 
     * @return Vector of loaded library paths
     */
    std::vector<std::string> GetLoadedLibraries() const;

private:
    // Platform-specific implementation helpers
    Result<std::vector<std::string>> GetSystemPaths() const;
    Result<std::vector<std::string>> ScanDirectory(const std::string& directory, 
                                                  const std::vector<std::string>& patterns) const;
    Result<std::string> GetFileVersion(const std::string& file_path) const;
    std::string FormatTimestamp(uint64_t timestamp) const;
    
    // Internal state
    std::map<std::string, LibraryHandle> loaded_libraries_;  ///< Path -> Handle mapping
    std::map<LibraryHandle, std::string> handle_to_path_;    ///< Handle -> Path mapping
    std::vector<std::string> search_paths_;                  ///< Cached search paths
    mutable std::mutex mutex_;                               ///< Thread safety
    
    // Library detection cache
    std::map<std::string, LibraryInfo> library_cache_;       ///< Cached library info
    bool cache_valid_ = false;                               ///< Cache validity flag
};

/**
 * @brief RAII wrapper for automatic library unloading
 */
class ScopedLibrary {
public:
    ScopedLibrary(RuntimeLoader& loader, LibraryHandle handle, const std::string& path)
        : loader_(loader), handle_(handle), path_(path) {}
    
    ~ScopedLibrary() {
        if (handle_) {
            loader_.UnloadLibrary(handle_);
        }
    }
    
    // Non-copyable
    ScopedLibrary(const ScopedLibrary&) = delete;
    ScopedLibrary& operator=(const ScopedLibrary&) = delete;
    
    // Movable
    ScopedLibrary(ScopedLibrary&& other) noexcept
        : loader_(other.loader_), handle_(other.handle_), path_(std::move(other.path_)) {
        other.handle_ = nullptr;
    }
    
    ScopedLibrary& operator=(ScopedLibrary&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                loader_.UnloadLibrary(handle_);
            }
            handle_ = other.handle_;
            path_ = std::move(other.path_);
            other.handle_ = nullptr;
        }
        return *this;
    }
    
    LibraryHandle GetHandle() const { return handle_; }
    const std::string& GetPath() const { return path_; }
    
    template<typename FuncType>
    Result<FuncType> GetSymbol(const std::string& symbol_name) {
        return loader_.GetTypedSymbol<FuncType>(handle_, symbol_name);
    }

private:
    RuntimeLoader& loader_;
    LibraryHandle handle_;
    std::string path_;
};

} // namespace kerntopia