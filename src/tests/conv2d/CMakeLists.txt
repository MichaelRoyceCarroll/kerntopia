# Convolution 2D Test

# Add SLANG kernel compilation for this test
add_slang_kernels_to_test("conv2d" ${CMAKE_CURRENT_SOURCE_DIR})

# Create Conv2D test library
add_library(kerntopia_conv2d_test STATIC 
    conv2d_test.cpp
    conv2d_core.cpp
)

target_link_libraries(kerntopia_conv2d_test
    PUBLIC
        kerntopia_test_common
)

target_include_directories(kerntopia_conv2d_test
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

# Register this test library globally so parent can link it
set_property(GLOBAL APPEND PROPERTY KERNTOPIA_TEST_LIBRARIES kerntopia_conv2d_test)

message(STATUS "Conv2D test - SLANG kernel compilation and test library created")