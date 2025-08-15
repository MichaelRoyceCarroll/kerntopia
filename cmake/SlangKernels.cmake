# SLANG Kernel Compilation Functions
# Provides utilities for compiling SLANG kernels to multiple targets

# Profile × Target combinations supported
# Format: backend:profile_or_capability:target
set(SLANG_TARGETS
    "vulkan:glsl_450:spirv"
    "cuda:cuda_sm_7_0:ptx"
)

# Create kernel output directory and staging directory for executables
set(KERNEL_OUTPUT_DIR ${CMAKE_BINARY_DIR}/kernels)
set(KERNEL_STAGING_DIR ${CMAKE_BINARY_DIR}/bin/kernels)
file(MAKE_DIRECTORY ${KERNEL_OUTPUT_DIR})
file(MAKE_DIRECTORY ${KERNEL_STAGING_DIR})

# Function to compile a single SLANG kernel for all targets
function(compile_slang_kernel KERNEL_NAME KERNEL_SOURCE_DIR)
    set(KERNEL_SOURCE_FILE ${KERNEL_SOURCE_DIR}/${KERNEL_NAME}.slang)
    
    if(NOT EXISTS ${KERNEL_SOURCE_FILE})
        message(WARNING "Kernel source file not found: ${KERNEL_SOURCE_FILE}")
        return()
    endif()
    
    # Compile for each target combination
    foreach(TARGET_COMBO ${SLANG_TARGETS})
        string(REPLACE ":" ";" TARGET_PARTS ${TARGET_COMBO})
        list(GET TARGET_PARTS 0 BACKEND)
        list(GET TARGET_PARTS 1 PROFILE)
        list(GET TARGET_PARTS 2 TARGET)
        
        # Generate output filename: conv2d-vulkan-glsl_450-spirv.spv
        if(${TARGET} STREQUAL "spirv")
            set(OUTPUT_EXTENSION "spv")
        elseif(${TARGET} STREQUAL "ptx")
            set(OUTPUT_EXTENSION "ptx")
        else()
            set(OUTPUT_EXTENSION ${TARGET})
        endif()
        
        set(OUTPUT_FILENAME "${KERNEL_NAME}-${BACKEND}-${PROFILE}-${TARGET}.${OUTPUT_EXTENSION}")
        set(OUTPUT_PATH ${KERNEL_OUTPUT_DIR}/${OUTPUT_FILENAME})
        set(METADATA_PATH ${KERNEL_OUTPUT_DIR}/${KERNEL_NAME}-${BACKEND}-${PROFILE}-${TARGET}.json)
        
        # Build slangc command  
        # Use -profile for Vulkan/DirectX, -capability for CUDA
        if(${BACKEND} STREQUAL "cuda")
            set(SLANG_FLAGS -target ${TARGET} -capability ${PROFILE} -stage compute -entry computeMain -o ${OUTPUT_PATH})
        else()
            set(SLANG_FLAGS -target ${TARGET} -profile ${PROFILE} -stage compute -entry computeMain -o ${OUTPUT_PATH})
        endif()
        
        message(STATUS "Creating build rule for: ${OUTPUT_PATH}")
        message(STATUS "SLANG command: ${SLANGC_EXECUTABLE} ${SLANG_FLAGS} ${KERNEL_SOURCE_FILE}")
        
        # Create custom target for this specific kernel compilation
        set(TARGET_NAME "${KERNEL_NAME}_${BACKEND}_${PROFILE}_${TARGET}")
        
        add_custom_target(${TARGET_NAME}
            COMMAND ${SLANGC_EXECUTABLE} ${SLANG_FLAGS} ${KERNEL_SOURCE_FILE}
            COMMAND ${CMAKE_COMMAND} -D KERNEL_NAME=${KERNEL_NAME}
                    -D BACKEND=${BACKEND} -D PROFILE=${PROFILE} -D TARGET=${TARGET}
                    -D SOURCE_FILE=${KERNEL_SOURCE_FILE} -D OUTPUT_FILE=${OUTPUT_PATH}
                    -D SLANG_VERSION=${SLANG_VERSION}
                    -D METADATA_FILE=${METADATA_PATH}
                    -P ${CMAKE_SOURCE_DIR}/cmake/GenerateKernelMetadata.cmake
            DEPENDS ${KERNEL_SOURCE_FILE}
            BYPRODUCTS ${OUTPUT_PATH} ${METADATA_PATH}
            COMMENT "Compiling ${KERNEL_NAME} for ${BACKEND} (${PROFILE} → ${TARGET})"
        )
        
        # Add to global kernel targets list  
        set_property(GLOBAL APPEND PROPERTY GLOBAL_COMPILED_KERNELS ${OUTPUT_PATH})
        set_property(GLOBAL APPEND PROPERTY GLOBAL_KERNEL_TARGETS ${TARGET_NAME})
        
        # Track source files for staging (only add once per source file)
        get_property(EXISTING_SOURCES GLOBAL PROPERTY GLOBAL_KERNEL_SOURCES)
        list(FIND EXISTING_SOURCES ${KERNEL_SOURCE_FILE} SOURCE_FOUND)
        if(SOURCE_FOUND EQUAL -1)
            set_property(GLOBAL APPEND PROPERTY GLOBAL_KERNEL_SOURCES ${KERNEL_SOURCE_FILE})
        endif()
    endforeach()
endfunction()

# Function to add SLANG kernel compilation to a test directory  
function(add_slang_kernels_to_test TEST_NAME TEST_SOURCE_DIR)
    # Find .slang files in the test directory
    file(GLOB KERNEL_SOURCES ${TEST_SOURCE_DIR}/*.slang)
    
    message(STATUS "Searching for SLANG files in: ${TEST_SOURCE_DIR}")
    message(STATUS "Found SLANG files: ${KERNEL_SOURCES}")
    
    foreach(KERNEL_SOURCE ${KERNEL_SOURCES})
        get_filename_component(KERNEL_NAME ${KERNEL_SOURCE} NAME_WE)
        message(STATUS "Setting up compilation for kernel: ${KERNEL_NAME}")
        compile_slang_kernel(${KERNEL_NAME} ${TEST_SOURCE_DIR})
    endforeach()
endfunction()

# Function to create kernels target with all compiled kernels
function(create_kernels_target)
    # This will be called after all tests have added their kernels
    get_property(ALL_KERNEL_TARGETS GLOBAL PROPERTY GLOBAL_KERNEL_TARGETS)
    
    message(STATUS "Creating kernels target depending on: ${ALL_KERNEL_TARGETS}")
    
    if(ALL_KERNEL_TARGETS)
        add_custom_target(kernels ALL)
        add_dependencies(kernels ${ALL_KERNEL_TARGETS})
        message(STATUS "Created kernels target depending on ${ALL_KERNEL_TARGETS}")
        
        # Copy kernels to staging directory for executables
        add_custom_command(
            TARGET kernels POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${KERNEL_OUTPUT_DIR} ${KERNEL_STAGING_DIR}
            COMMENT "Staging kernels to bin/kernels/ for executable access"
        )
        
        # Copy source SLANG files to staging directory for auditing
        get_property(ALL_KERNEL_SOURCES GLOBAL PROPERTY GLOBAL_KERNEL_SOURCES)
        if(ALL_KERNEL_SOURCES)
            add_custom_command(
                TARGET kernels POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy ${ALL_KERNEL_SOURCES} ${KERNEL_STAGING_DIR}
                COMMENT "Copying SLANG source files for audit trail"
            )
        endif()
        
        # Generate compile_commands.txt for educational use
        add_custom_command(
            TARGET kernels POST_BUILD
            COMMAND ${CMAKE_COMMAND} -D KERNEL_OUTPUT_DIR=${KERNEL_OUTPUT_DIR}
                    -P ${CMAKE_SOURCE_DIR}/cmake/GenerateCompileCommands.cmake
            COMMENT "Generating compile_commands.txt for kernel reproduction"
        )
    else()
        message(STATUS "No kernel targets found - kernels target not created")
    endif()
endfunction()

# Export variables for use in other CMake files
set(KERNEL_OUTPUT_DIR ${KERNEL_OUTPUT_DIR} CACHE PATH "Kernel output directory")
set(SLANG_TARGETS "${SLANG_TARGETS}" CACHE STRING "Supported SLANG target combinations")