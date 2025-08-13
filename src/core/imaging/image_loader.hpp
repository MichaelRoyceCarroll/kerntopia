#pragma once

#include "../common/error_handling.hpp"
#include "../common/test_params.hpp"
#include "image_data.hpp"
#include <string>
#include <vector>

namespace kerntopia {

/**
 * @brief Image loading and processing system
 * 
 * Provides image I/O capabilities for testing framework
 */
class ImageLoader {
public:
    ImageLoader() = default;
    ~ImageLoader() = default;
    
    /**
     * @brief Initialize image processing system
     * 
     * @return Success result
     */
    Result<void> Initialize();
    
    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();
    
    /**
     * @brief Load image from file
     * 
     * @param path Image file path
     * @param format Expected image format
     * @return Loaded image data or error
     */
    Result<ImageData> LoadImage(const std::string& path, ImageFormat format);
    
    /**
     * @brief Save image to file
     * 
     * @param image Image data to save
     * @param path Output file path
     * @return Success result
     */
    Result<void> SaveImage(const ImageData& image, const std::string& path);

private:
    bool initialized_ = false;
};

} // namespace kerntopia