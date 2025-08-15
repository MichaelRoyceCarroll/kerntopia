#include "path_utils.hpp"
#include <filesystem>
#include <stdexcept>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <limits.h>
#endif

namespace kerntopia {

std::string PathUtils::GetExecutablePath() {
#ifdef _WIN32
    char path[MAX_PATH];
    DWORD result = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (result == 0 || result == MAX_PATH) {
        throw std::runtime_error("Failed to get executable path on Windows");
    }
    return std::string(path);
#else
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len == -1) {
        throw std::runtime_error("Failed to get executable path on Linux");
    }
    path[len] = '\0';
    return std::string(path);
#endif
}

std::string PathUtils::GetExecutableDirectory() {
    std::string exe_path = GetExecutablePath();
    std::filesystem::path fs_path(exe_path);
    std::filesystem::path dir_path = fs_path.parent_path();
    return dir_path.string();
}

std::string PathUtils::GetKernelsDirectory() {
    // Get executable directory (e.g., "/path/to/build/bin")
    std::string exe_dir = GetExecutableDirectory();
    
    // Convert to filesystem path and get parent (e.g., "/path/to/build")
    std::filesystem::path exe_dir_path(exe_dir);
    std::filesystem::path build_dir = exe_dir_path.parent_path();
    
    // Construct kernels directory path (e.g., "/path/to/build/kernels")
    std::filesystem::path kernels_dir = build_dir / "kernels";
    
    // Return with trailing slash for consistency
    return kernels_dir.string() + "/";
}

} // namespace kerntopia