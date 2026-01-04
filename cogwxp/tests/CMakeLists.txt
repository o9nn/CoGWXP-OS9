# CoGWXP-OS9 Test Suite CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

# Test executable
add_executable(cogwxp-tests
    test_cogwxp.c
)

# Link against all cogwxp libraries
target_link_libraries(cogwxp-tests
    cogwxp-cogutil
    cogwxp-atomspace
    cogwxp-pln
    cogwxp-9p
    cogwxp-cogw7os
    pthread
)

# Include directories
target_include_directories(cogwxp-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/cogwxp
)

# Enable testing
enable_testing()

# Add test
add_test(NAME cogwxp-tests COMMAND cogwxp-tests)

# Set test properties
set_tests_properties(cogwxp-tests PROPERTIES
    TIMEOUT 60
    LABELS "unit"
)

# Custom target to run tests
add_custom_target(run-tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS cogwxp-tests
    COMMENT "Running CoGWXP-OS9 test suite"
)
