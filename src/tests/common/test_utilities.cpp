#include "test_utilities.hpp"

namespace kerntopia {
namespace test_utils {

TestConfiguration CreateDefaultConfig() {
    TestConfiguration config;
    // Set default values for testing
    config.target_backend = Backend::CPU;
    config.device_id = 0;
    config.mode = TestMode::FUNCTIONAL;
    config.iterations = 1;
    config.size = TestSize::HD_1080P;
    config.format = ImageFormat::RGB8;
    config.validate_output = false; // Disabled for placeholder implementation
    config.save_output = false;
    return config;
}

bool ValidateResult(const KernelResult& result) {
    // Placeholder implementation
    return result.success;
}

} // namespace test_utils
} // namespace kerntopia