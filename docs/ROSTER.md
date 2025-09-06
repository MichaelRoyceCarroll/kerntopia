# Kerntopia Kernel Roster

**Complete catalog of GPU compute kernels available in the Kerntopia benchmarking suite.**

## Status: Placeholder

This roster file is currently under development and will be expanded as the project grows.

## Implementation Status

**Legend:**
- ✅ **Implemented** - Fully working with CUDA + Vulkan backends
- 🚧 **In Progress** - Under development
- 📋 **Planned** - Designed and scheduled for implementation
- 💡 **Proposed** - Ideas under consideration

---

## Image Processing Kernels

### ✅ Convolution (2D)
**Status:** Fully implemented  
**Backends:** CUDA, Vulkan  
**Location:** [src/tests/conv2d/](../src/tests/conv2d/)

**Description:**  
Standard 2D image convolution with Gaussian blur kernel. Demonstrates fundamental image processing concepts with optimized GPU memory access patterns.

---

### 💡 Bilateral Filter
**Status:** Planned for v0.2.0  
**Target Backends:** CUDA, Vulkan

**Description:**  
Edge-preserving smoothing filter that reduces noise while maintaining sharp boundaries. Essential for photography and computer vision applications.

---

### 💡 Sobel Edge Detection  
**Status:** Planned for v0.2.0  
**Target Backends:** CUDA, Vulkan

**Description:**
Classical edge detection using Sobel operators. Fundamental computer vision kernel demonstrating gradient computation on GPUs.

---

### 💡 Histogram Computation
**Status:** Planned for v0.3.0  
**Target Backends:** CUDA, Vulkan

**Description:**
Parallel histogram computation with atomic operations. Demonstrates GPU synchronization primitives and memory access optimization.


---

### 💡 Gaussian Pyramid
**Status:** Under consideration  
**Target Backends:** CUDA, Vulkan

**Description:**
Multi-scale image representation for computer vision and image compression applications.

---

## Computer Vision Kernels

### 💡 Harris Corner Detection
**Status:** Planned for v0.3.0  
**Target Backends:** CUDA, Vulkan

**Description:**
Feature point detection algorithm essential for image matching and tracking applications.

---

### 💡 Optical Flow (Lucas-Kanade)
**Status:** Planned for v0.4.0  
**Target Backends:** CUDA, Vulkan

**Description:**
Motion estimation between image frames. Advanced kernel showcasing iterative GPU algorithms.

---

### 💡 Canny Edge Detection
**Status:** Under consideration  
**Target Backends:** CUDA, Vulkan

**Description:**
Multi-stage edge detection with hysteresis thresholding. Combines Gaussian blur, Sobel operators, and connected component analysis.

---

### 💡 Background Subtraction
**Status:** Under consideration  
**Target Backends:** CUDA, Vulkan

**Description:**
Video processing technique for motion detection and object tracking applications.

---

## Linear Algebra Kernels

### 💡 Parallel Reduction (Sum/Max/Min)
**Status:** Planned for v0.2.0  
**Target Backends:** CUDA, Vulkan

**Description:**
Fundamental parallel algorithm demonstrating efficient tree-based reduction on GPUs. Essential building block for many algorithms.

---

### 💡 Parallel Prefix Scan
**Status:** Planned for v0.2.0  
**Target Backends:** CUDA, Vulkan

**Description:**
Inclusive and exclusive scan operations. Critical building block for many parallel algorithms including sorting and stream compaction.

---

### 💡 Dense Matrix Transpose
**Status:** Planned for v0.3.0  
**Target Backends:** CUDA, Vulkan

**Description:**
Memory-efficient matrix transposition demonstrating tiled algorithms and memory coalescing optimization.

---

## Sorting & Search Kernels

### 💡 Radix Sort
**Status:** Planned for v0.4.0  
**Target Backends:** CUDA, Vulkan

**Description:**
High-performance parallel sorting algorithm. Demonstrates complex multi-pass GPU algorithms with global synchronization.

---

### 💡 Merge Sort (Parallel)
**Status:** Under consideration  
**Target Backends:** CUDA, Vulkan

**Description:**
Divide-and-conquer sorting with parallel merge operations.

---

## Signal Processing Kernels

### 💡 2D FFT
**Status:** Planned for v0.5.0  
**Target Backends:** CUDA, Vulkan

**Description:**
Two-dimensional Fast Fourier Transform implementation. Advanced kernel showcasing complex number arithmetic and algorithmic optimization.

---

## Physics & Simulation Kernels

### 💡 N-Body Simulation (Simple)
**Status:** Planned for v0.3.0  
**Target Backends:** CUDA, Vulkan

**Description:**
Gravitational N-body physics simulation. Demonstrates compute-intensive algorithms and numerical integration on GPUs.

---

### 💡 Particle System Update
**Status:** Under consideration  
**Target Backends:** CUDA, Vulkan

**Description:**
Real-time particle dynamics for graphics and simulation applications.

---

## Procedural Generation Kernels

### 💡 Perlin Noise Generation
**Status:** Under consideration  
**Target Backends:** CUDA, Vulkan

**Description:**
Procedural texture generation using Perlin noise algorithms.

---

### 💡 Voronoi Diagram Generation
**Status:** Under consideration  
**Target Backends:** CUDA, Vulkan

**Description:**
Computational geometry algorithm for spatial partitioning and procedural content generation.

---

### 💡 Monte Carlo Forest Layout
**Status:** Planned for v0.5.0  
**Target Backends:** CUDA, Vulkan

**Description:**
Procedural forest generation using Monte Carlo methods. Demonstrates random number generation and spatial algorithms on GPUs.

---

## Development Templates

### 🚧 Vector Add Example
**Status:** Placeholder 
**Backends:** CUDA, Vulkan  
**Location:** [examples/vector_add/](../examples/vector_add/)

**Description:**  
Simple element-wise vector addition. Perfect starting point for developers wanting to add their own kernels to Kerntopia.

**Features:**
- Complete CMake integration example
- SLANG kernel template
- Both standalone and suite mode integration
- Comprehensive documentation

---

## Performance Benchmarking

Each implemented kernel includes:

- **Functional Tests**: Correctness validation with reference implementations
- **Performance Tests**: Scaling analysis across problem sizes where applicable
- **Backend Comparison**: Direct CUDA vs Vulkan performance comparison
- **Memory Analysis**: Bandwidth utilization and optimization opportunities

## Adding New Kernels

Interested in contributing a kernel? Check out:

1. **[Vector Add Template](../examples/vector_add/)** - Complete starting point
2. **[Developer Guide](DEVELOPER_GUIDE.md)** - Integration instructions
3. **[Contributing Guidelines](../CONTRIBUTING.md)** - Submission process

---

**Version:** 0.1.0  
**Last Updated:** 20250905  
**Total Kernels:** 1 implemented, 15+ planned

*Remember: Every expert was once a beginner.* ✨🌾✨