# Kerntopia Suite Mode Executable
# Main C++ executable that orchestrates all kernel tests via GTest framework

set(SUITE_SOURCES
    main.cpp
    kernel_manager.cpp
    command_line.cpp
    test_orchestrator.cpp
)

set(SUITE_HEADERS
    kernel_manager.hpp
    command_line.hpp
    test_orchestrator.hpp
)

# Create main suite executable
add_executable(kerntopia ${SUITE_SOURCES} ${SUITE_HEADERS})

# Link dependencies (including test libraries for embedded GTest execution)
target_link_libraries(kerntopia
    PRIVATE
        kerntopia_core
        kerntopia_test_common  # Test framework
        kerntopia_conv2d_test  # Conv2D test library (explicit)
        gtest
        gtest_main
        Threads::Threads
)

# Include directories
target_include_directories(kerntopia
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compiler definitions
target_compile_definitions(kerntopia
    PRIVATE
        KERNTOPIA_SUITE_MODE
)

# Output directory
set_target_properties(kerntopia PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Installation
install(TARGETS kerntopia
    RUNTIME DESTINATION bin
)