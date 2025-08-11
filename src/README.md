# Kerntopia Source Code

This directory contains the main source code for the Kerntopia SLANG-centric GPU benchmarking suite.

## Structure

- **`kerntopia/`** - Main suite mode executable that orchestrates all kernel tests
- **`core/`** - Shared infrastructure used by both suite mode and standalone executables  
- **`tests/`** - Individual kernel test implementations and standalone executables

## Core Components

The `core/` directory provides the foundation for all Kerntopia functionality:

### Backend Abstraction (`core/backend/`)
- **`ikernel_runner.hpp`** - Abstract interface for GPU kernel execution
- **`backend_factory.hpp`** - Factory for creating backend instances with dynamic loading
- **`cuda_runner.hpp/.cpp`** - CUDA backend implementation
- **`vulkan_runner.hpp/.cpp`** - Vulkan backend implementation  
- **`runtime_loader.hpp/.cpp`** - Cross-platform dynamic library loading

### Common Utilities (`core/common/`)
- **`data_span.hpp`** - C++17/20 hybrid span implementation for memory views
- **`logger.hpp/.cpp`** - Thread-safe logging system with component categorization
- **`error_handling.hpp/.cpp`** - Comprehensive error handling with Result<T> types
- **`kernel_result.hpp`** - Performance timing and validation result structures
- **`test_params.hpp`** - Test configuration and parameter management

### System Integration (`core/system/`)
- **`interrogator.hpp/.cpp`** - Comprehensive system and device capability detection
- **`device_info.hpp`** - Device information and capability structures

### Image Processing (`core/imaging/`)
- **`image_loader.hpp/.cpp`** - STB and TinyEXR integration for multi-format support
- **`color_space.hpp/.cpp`** - Color space conversion utilities

## Build Integration

The core library is built as a static library (`kerntopia_core`) that is linked into:
- Main suite executable (`kerntopia`)
- Individual standalone test executables (`kerntopia-conv2d`, etc.)
- Example projects and developer templates

## Design Principles

- **Brevity over verbosity** - Core project principle
- **Cross-platform compatibility** - Linux primary, Windows secondary
- **Dynamic backend loading** - Runtime detection without static dependencies
- **Educational focus** - Comprehensive commenting and clear structure
- **Type safety** - Strong typing with Result<T> for error handling