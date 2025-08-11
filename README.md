# Kerntopia: SLANG-Centric GPU Benchmarking Suite

A comprehensive SLANG-centric GPU compute benchmarking and testing suite designed to showcase canonical GPU computing kernels. Kerntopia targets GPU developers, SIGGRAPH attendees, hardware platform hiring managers, and GPU computing enthusiasts who want to learn SLANG as a disruptive platform language.

## Features

- **Multi-backend Support**: CUDA, Vulkan (with future DX12, CPU)
- **Three Operational Modes**: Suite mode, standalone executables, Python wrapper
- **Dynamic Runtime Loading**: Backends loaded at runtime, not link-time
- **System Interrogation**: Comprehensive hardware/software capability detection
- **Reproducible Results**: Complete audit trail with checksums, timestamps, file paths
- **JIT and Precompile Modes**: Flexible compilation approaches
- **Performance & Functional Testing**: GTest framework with statistical analysis

## Quick Start

### Prerequisites

**System Requirements:**
- Linux (WSL2 supported) or Windows
- CMake 3.20+
- C++17/20 compatible compiler (GCC 9+, Clang 10+, MSVC 2019+)

**GPU Backends:**
- **CUDA**: NVIDIA drivers + CUDA Toolkit 11.0+ (for NV4060+ hardware)
- **Vulkan**: Modern Vulkan drivers (supports CPU llvmpipe for development)

### Build Instructions

```bash
# Clone and build
git clone <repository-url> kerntopia
cd kerntopia
mkdir build && cd build

# Configure (C++17 mode)
cmake ..

# Or enable C++20 for std::span
cmake -DKERNTOPIA_USE_CPP20=ON ..

# Build
make -j$(nproc)
```

### Running Tests

```bash
# Suite mode - run all kernels
./bin/kerntopia run all

# Suite mode - specific kernels
./bin/kerntopia run conv2d,reduction --backend cuda

# Standalone mode
./bin/kerntopia-conv2d --input assets/images/test_1080p.png --output result.png

# Python wrapper
cd ../python
python kerntopia-harness.py run all --backends cuda,vulkan
```

## Kernel Roster

### Image Processing
- **Convolution 2D** - Standard image convolution with configurable kernels
- **Bilateral Filter** - Edge-preserving smoothing filter

### Linear Algebra  
- **Parallel Reduction** - Sum/Max/Min operations with optimized GPU patterns
- **Matrix Transpose** - Efficient matrix transposition with memory coalescing

### Examples
- **Vector Add** - Template for adding new kernels

See [ROSTER.md](docs/ROSTER.md) for detailed kernel descriptions and performance characteristics.

## System Interrogation

Kerntopia provides comprehensive system analysis rivaling `nvidia-smi` and `clinfo`:

```bash
# System overview
./bin/kerntopia info --system

# Detailed device information
./bin/kerntopia info --devices --verbose

# Runtime detection
./bin/kerntopia info --runtimes
```

## Documentation

- [Build Instructions](docs/BUILD.md) - Detailed build setup for all platforms
- [Usage Guide](docs/USAGE.md) - Command-line reference and examples
- [Adding Kernels](docs/ADDING_KERNELS.md) - Developer guide for new kernels
- [Kernel Catalog](docs/ROSTER.md) - Complete kernel descriptions and benchmarks

## Development

Want to add your own kernel? Start with the [basic vector add example](examples/basic_vector_add/):

```bash
cd examples/basic_vector_add
mkdir build && cd build
cmake ..
make
./vector_add_standalone --help
```

## Architecture

Kerntopia uses a modular architecture with three operational modes:

1. **Suite Mode** (`kerntopia`) - Complete test orchestration with GTest framework
2. **Standalone Mode** (`kerntopia-*`) - Individual kernel executables for focused analysis  
3. **Wrapper Mode** - Python harness with advanced reporting and result serialization

The system features dynamic backend loading, comprehensive audit trails, and educational documentation throughout.

## License

*License to be determined - currently in development phase*

## Contributing

This project is designed as an educational resource. While not currently accepting external contributions, the codebase serves as a comprehensive example of SLANG kernel development and GPU compute best practices.

## Attribution

**Third-Party Libraries:**
- [STB Libraries](third-party/stb/) - Image I/O (MIT/Public Domain)
- [TinyEXR](third-party/tinyexr/) - HDR image support (BSD)

See individual library directories for complete license information.

---

**Version:** 0.1.0  
**Target Audience:** GPU developers, SIGGRAPH attendees, hardware platform hiring managers, GPU computing enthusiasts