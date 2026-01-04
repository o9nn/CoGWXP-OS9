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

# Niche Construction & Beast Mode test executable
add_executable(test-niche-beast
    test_niche_beast.c
)

# Link libraries for niche/beast tests
target_link_libraries(test-niche-beast
    cogwxp-cogutil
    cogwxp-atomspace
    cogwxp-pln
    pthread
    m
)

# Include directories
target_include_directories(test-niche-beast PRIVATE
    ${CMAKE_SOURCE_DIR}/cogwxp
)

# Enable testing
enable_testing()

# Add tests
add_test(NAME cogwxp-tests COMMAND cogwxp-tests)
add_test(NAME test-niche-beast COMMAND test-niche-beast)

# Set test properties
set_tests_properties(cogwxp-tests PROPERTIES
    TIMEOUT 60
    LABELS "unit"
)

set_tests_properties(test-niche-beast PROPERTIES
    TIMEOUT 120
    LABELS "integration"
)

# Custom target to run tests
add_custom_target(run-tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS cogwxp-tests test-niche-beast
    COMMENT "Running CoGWXP-OS9 test suite"
)
