#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace kerntopia {

/**
 * @brief Extended device information structures
 * 
 * Placeholder for Phase 1 system interrogation implementation
 */
struct ExtendedDeviceInfo {
    std::string name;
    std::string vendor;
    std::string driver_version;
    uint64_t memory_bytes = 0;
    
    // Additional device properties can be added here
    // This is a placeholder structure for the system interrogation phase
};

} // namespace kerntopia