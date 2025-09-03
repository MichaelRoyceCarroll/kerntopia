# Kerntopia Core Library
# Shared infrastructure used by both suite mode and standalone executables

set(CORE_SOURCES
    # Common utilities
    common/logger.cpp
    common/error_handling.cpp
    common/data_span.cpp
    common/path_utils.cpp
    
    # Backend abstraction
    backend/backend_factory.cpp
    backend/cuda_runner.cpp
    backend/cuda_memory.cpp
    backend/vulkan_runner.cpp
    backend/vulkan_memory.cpp
    backend/runtime_loader.cpp
    
    # Imaging pipeline
    imaging/image_loader.cpp
    imaging/color_space.cpp
    
    # System interrogation
    system/interrogator.cpp
    system/device_info.cpp
    system/system_interrogator.cpp
    system/system_info_service.cpp
)

set(CORE_HEADERS
    # Common utilities
    common/logger.hpp
    common/error_handling.hpp
    common/data_span.hpp
    common/path_utils.hpp
    common/kernel_result.hpp
    common/test_params.hpp
    
    # Backend abstraction
    backend/ikernel_runner.hpp
    backend/backend_factory.hpp
    backend/cuda_runner.hpp
    backend/cuda_memory.hpp
    backend/vulkan_runner.hpp
    backend/vulkan_memory.hpp
    backend/runtime_loader.hpp
    
    # Imaging pipeline
    imaging/image_loader.hpp
    imaging/color_space.hpp
    
    # System interrogation
    system/interrogator.hpp
    system/device_info.hpp
    system/system_interrogator.hpp
    system/interrogation_data.hpp
    system/system_info_service.hpp
)

# Create core library
add_library(kerntopia_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})

# Link dependencies
target_link_libraries(kerntopia_core
    PUBLIC
        Threads::Threads
    PRIVATE
        ${CMAKE_DL_LIBS}  # For dynamic loading
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(kerntopia_core PRIVATE kernel32)
elseif(UNIX)
    target_link_libraries(kerntopia_core PRIVATE dl)
endif()

# Include directories
target_include_directories(kerntopia_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${KERNTOPIA_THIRD_PARTY_DIR}/stb
        ${KERNTOPIA_THIRD_PARTY_DIR}/tinyexr
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add Vulkan headers if SDK is available
if(DEFINED ENV{VULKAN_SDK})
    target_include_directories(kerntopia_core
        PUBLIC
            ${VULKAN_INCLUDE_DIR}
    )
    target_compile_definitions(kerntopia_core
        PUBLIC
            KERNTOPIA_VULKAN_SDK_AVAILABLE
    )
endif()

# Add CUDA headers if found
if(CUDA_HEADER_FOUND)
    target_include_directories(kerntopia_core
        PUBLIC
            ${CUDA_INCLUDE_DIR}
    )
    target_compile_definitions(kerntopia_core
        PUBLIC
            KERNTOPIA_CUDA_SDK_AVAILABLE
    )
endif()

# Compiler definitions
target_compile_definitions(kerntopia_core
    PRIVATE
        KERNTOPIA_VERSION_MAJOR=0
        KERNTOPIA_VERSION_MINOR=1
        KERNTOPIA_VERSION_PATCH=0
)

# Export headers for installation
set_target_properties(kerntopia_core PROPERTIES
    PUBLIC_HEADER "${CORE_HEADERS}"
)

# Installation
install(TARGETS kerntopia_core
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/kerntopia/core
)