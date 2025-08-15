cmake_minimum_required(VERSION 3.20)
project(Kerntopia 
    VERSION 0.1.0
    DESCRIPTION "SLANG-centric GPU benchmarking suite"
    LANGUAGES CXX)

# C++ Standard Support
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Option to enable C++20 mode for std::span
option(KERNTOPIA_USE_CPP20 "Enable C++20 mode for std::span support" OFF)

if(KERNTOPIA_USE_CPP20)
    set(CMAKE_CXX_STANDARD 20)
    add_compile_definitions(KERNTOPIA_USE_CPP20)
endif()

# Build Configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Platform-specific settings
if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Find required packages
find_package(Threads REQUIRED)

# Include FetchContent for dependencies
include(FetchContent)

# Fetch GTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Fetch SLANG Compiler
set(SLANG_VERSION "2025.14.3" CACHE STRING "Slang version to fetch")
message(STATUS "Fetching SLANG ${SLANG_VERSION} (this will download ~50MB)")

FetchContent_Declare(
    slang
    URL https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-linux-x86_64-glibc-2.27.tar.gz
    DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/_deps/slang-download
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(slang)

# Set SLANG paths
set(SLANG_ROOT ${slang_SOURCE_DIR})
set(SLANG_BINARY_DIR ${SLANG_ROOT}/bin)
set(SLANG_INCLUDE_DIR ${SLANG_ROOT}/include) 
set(SLANG_LIBRARY_DIR ${SLANG_ROOT}/lib)

# Find slangc executable
find_program(SLANGC_EXECUTABLE
    NAMES slangc slangc.exe
    PATHS ${SLANG_BINARY_DIR}
    NO_DEFAULT_PATH
    REQUIRED
)

# Find slang library for JIT mode (future)
find_library(SLANG_LIBRARY
    NAMES slang libslang
    PATHS ${SLANG_LIBRARY_DIR}
    NO_DEFAULT_PATH
)

if(SLANGC_EXECUTABLE)
    message(STATUS "Found slangc: ${SLANGC_EXECUTABLE}")
else()
    message(FATAL_ERROR "Could not find slangc executable")
endif()

if(SLANG_LIBRARY)
    message(STATUS "Found slang library: ${SLANG_LIBRARY}")
else()
    message(WARNING "Could not find slang library (JIT mode will be unavailable)")
endif()

# Third-party include directories
set(KERNTOPIA_THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/third-party)
include_directories(${KERNTOPIA_THIRD_PARTY_DIR}/stb)
include_directories(${KERNTOPIA_THIRD_PARTY_DIR}/tinyexr)

# Global include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# SLANG kernel compilation functions
include(${CMAKE_SOURCE_DIR}/cmake/SlangKernels.cmake)

# Core library
add_subdirectory(src/core)

# Main suite executable
add_subdirectory(src/kerntopia)

# Standalone kernel executables
# Conv2D standalone executable
add_executable(kerntopia-conv2d src/tests/conv2d/main.cpp)

target_link_libraries(kerntopia-conv2d
    PRIVATE
        kerntopia_core
        kerntopia_conv2d_test  # For Conv2dCore shared code
        Threads::Threads
)

target_include_directories(kerntopia-conv2d
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(kerntopia-conv2d
    PRIVATE
        KERNTOPIA_STANDALONE_MODE
)

# Output directory
set_target_properties(kerntopia-conv2d PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Test executables
add_subdirectory(src/tests)

# Examples (optional)
option(KERNTOPIA_BUILD_EXAMPLES "Build example projects" ON)
if(KERNTOPIA_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Create kernels target after all subdirectories have been processed
create_kernels_target()

# Deploy assets to build directory (parallel to bin/ and kernels/)
file(COPY ${CMAKE_SOURCE_DIR}/assets/
     DESTINATION ${CMAKE_BINARY_DIR}/assets
     FILES_MATCHING 
     PATTERN "*.png"
     PATTERN "*.exr" 
     PATTERN "*.bin"
     PATTERN "*.txt")

# Installation
install(TARGETS kerntopia kerntopia-conv2d
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)

# Asset installation
install(DIRECTORY assets/
    DESTINATION share/kerntopia/assets
    FILES_MATCHING 
    PATTERN "*.png"
    PATTERN "*.exr"
    PATTERN "*.bin")

# Documentation installation
install(DIRECTORY docs/
    DESTINATION share/kerntopia/docs
    FILES_MATCHING PATTERN "*.md")

# Python wrapper
install(DIRECTORY python/
    DESTINATION share/kerntopia/python
    FILES_MATCHING 
    PATTERN "*.py"
    PATTERN "*.txt")