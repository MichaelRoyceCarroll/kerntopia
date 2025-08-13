# Kerntopia Test Framework
# Individual kernel tests and standalone executables

# Common test infrastructure
set(TEST_COMMON_SOURCES
    common/base_test.cpp
    common/gtest_main.cpp
    common/test_utilities.cpp
)

set(TEST_COMMON_HEADERS
    common/base_test.hpp
    common/test_utilities.hpp
)

# Create common test library
add_library(kerntopia_test_common STATIC 
    ${TEST_COMMON_SOURCES} 
    ${TEST_COMMON_HEADERS}
)

target_link_libraries(kerntopia_test_common
    PUBLIC
        kerntopia_core
        gtest
        gtest_main
)

target_include_directories(kerntopia_test_common
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add individual kernel test directories
add_subdirectory(vector_add)
add_subdirectory(conv2d)
add_subdirectory(bilateral)
add_subdirectory(reduction)
add_subdirectory(transpose)

# Note: Tests are now linked directly into the main kerntopia executable
# No separate test executable needed

# Installation
install(TARGETS kerntopia_test_common
    ARCHIVE DESTINATION lib
)