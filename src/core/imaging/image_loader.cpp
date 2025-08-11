#include "image_loader.hpp"

namespace kerntopia {

Result<void> ImageLoader::Initialize() {
    // Placeholder implementation for Phase 2
    initialized_ = true;
    return KERNTOPIA_VOID_SUCCESS();
}

void ImageLoader::Shutdown() {
    initialized_ = false;
}

} // namespace kerntopia