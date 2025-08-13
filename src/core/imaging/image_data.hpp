#pragma once

#include <vector>
#include <cstdint>

namespace kerntopia {

/**
 * @brief Raw image data container
 */
struct ImageData {
    uint32_t width = 0;             ///< Image width in pixels
    uint32_t height = 0;            ///< Image height in pixels
    uint32_t channels = 0;          ///< Number of channels (1=grayscale, 3=RGB, 4=RGBA)
    uint32_t bits_per_channel = 8;  ///< Bits per channel (8, 16, 32)
    std::vector<uint8_t> data;      ///< Raw pixel data
    
    /**
     * @brief Calculate total size in bytes
     */
    size_t GetSizeBytes() const {
        return width * height * channels * (bits_per_channel / 8);
    }
    
    /**
     * @brief Check if image data is valid
     */
    bool IsValid() const {
        return width > 0 && height > 0 && channels > 0 && 
               data.size() == GetSizeBytes();
    }
    
    /**
     * @brief Create empty image with given dimensions
     */
    static ImageData Create(uint32_t w, uint32_t h, uint32_t c, uint32_t bpc = 8) {
        ImageData image;
        image.width = w;
        image.height = h;
        image.channels = c;
        image.bits_per_channel = bpc;
        image.data.resize(image.GetSizeBytes());
        return image;
    }
};

} // namespace kerntopia