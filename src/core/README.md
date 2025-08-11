# Kerntopia Core Infrastructure

The core library provides shared infrastructure for all Kerntopia execution modes. This includes backend abstraction, system interrogation, logging, error handling, and image processing capabilities.

## Components

### Backend Abstraction Layer
Cross-platform GPU abstraction supporting multiple compute APIs:
- Dynamic runtime library detection and loading
- Unified kernel execution interface
- Device enumeration and capability reporting
- Performance timing with separated memory/compute phases

### Common Utilities
Foundation components used throughout the codebase:
- **Data Span**: C++17/20 compatible memory views
- **Logging**: Thread-safe, categorized logging system
- **Error Handling**: Result<T> types with comprehensive error information
- **Test Parameters**: Flexible configuration and parameter management

### System Interrogation
Comprehensive system analysis capabilities:
- GPU device detection (NVIDIA, Intel, AMD)
- Runtime library enumeration with version detection
- Duplicate detection with primary/secondary identification
- Complete audit trail generation

### Image Processing Pipeline  
Multi-format image I/O and processing:
- STB library integration for standard formats
- TinyEXR integration for HDR/OpenEXR support
- Color space conversion utilities
- Memory-efficient processing for 4K+ assets

## Usage

The core library is designed to be included by:
```cpp
#include "core/backend/backend_factory.hpp"
#include "core/common/logger.hpp"
#include "core/common/error_handling.hpp"
// ... other core headers as needed
```

Link against the `kerntopia_core` static library in CMake:
```cmake
target_link_libraries(your_target PRIVATE kerntopia_core)
```

## Key Features

- **Dynamic Loading**: GPU backends loaded at runtime using LoadLibraryEx/dlopen
- **Graceful Degradation**: Continues operation when backends unavailable
- **Comprehensive Logging**: Component-tagged messages with file/console output
- **Type Safety**: Result<T> error handling prevents exception-based control flow
- **Educational Focus**: Extensive documentation and clear error messages

## Thread Safety

All core components are designed for multi-threaded use:
- Logger uses mutex protection for concurrent access
- Backend factory is thread-safe for device enumeration
- Error handling structures are immutable after construction
- Runtime loader protects dynamic library operations