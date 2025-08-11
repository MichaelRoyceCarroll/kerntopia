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

# Third-party include directories
set(KERNTOPIA_THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/third-party)
include_directories(${KERNTOPIA_THIRD_PARTY_DIR}/stb)
include_directories(${KERNTOPIA_THIRD_PARTY_DIR}/tinyexr)

# Global include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# Core library
add_subdirectory(src/core)

# Main suite executable
add_subdirectory(src/kerntopia)

# Test executables
add_subdirectory(src/tests)

# Examples (optional)
option(KERNTOPIA_BUILD_EXAMPLES "Build example projects" ON)
if(KERNTOPIA_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
install(TARGETS kerntopia
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