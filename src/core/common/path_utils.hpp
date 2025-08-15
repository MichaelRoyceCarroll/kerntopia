#pragma once

#include <string>

namespace kerntopia {

/**
 * @brief Utility functions for path resolution
 */
class PathUtils {
public:
    /**
     * @brief Get the absolute path to the directory containing the current executable
     * @return Absolute path to executable directory (e.g., "/path/to/build/bin/")
     */
    static std::string GetExecutableDirectory();
    
    /**
     * @brief Get the absolute path to the kernels directory relative to the current executable
     * @return Absolute path to kernels directory (e.g., "/path/to/build/kernels/")
     */
    static std::string GetKernelsDirectory();
    
private:
    /**
     * @brief Get the absolute path to the current executable
     * @return Absolute path to executable (e.g., "/path/to/build/bin/kerntopia")
     */
    static std::string GetExecutablePath();
};

} // namespace kerntopia