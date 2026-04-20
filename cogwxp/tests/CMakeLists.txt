# CoGWXP-OS9 Test Suite CMakeLists.txt
#
# Test executables and their CTest registrations.
# Labels:
#   unit        — fast isolated unit tests   (<  60 s)
#   integration — multi-component tests      (< 120 s)
#   stress      — load / concurrency tests   (< 600 s)

cmake_minimum_required(VERSION 3.16)

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Helper macro: build a test executable and register it with CTest
# ---------------------------------------------------------------------------
macro(cogwxp_add_test TARGET_NAME SOURCES LABELS TIMEOUT)
    add_executable(${TARGET_NAME} ${SOURCES})
    target_link_libraries(${TARGET_NAME}
        cogwxp-cogutil
        cogwxp-atomspace
        cogwxp-pln
        cogwxp-9p
        cogwxp-cogw7os
        Threads::Threads
        m
    )
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/cogwxp
    )
    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
    set_tests_properties(${TARGET_NAME} PROPERTIES
        TIMEOUT  ${TIMEOUT}
        LABELS   "${LABELS}"
    )
endmacro()

# Enable testing
enable_testing()

# ---------------------------------------------------------------------------
# Original tests (preserved)
# ---------------------------------------------------------------------------

add_executable(cogwxp-tests
    test_cogwxp.c
)
target_link_libraries(cogwxp-tests
    cogwxp-cogutil
    cogwxp-atomspace
    cogwxp-pln
    cogwxp-9p
    cogwxp-cogw7os
    Threads::Threads
)
target_include_directories(cogwxp-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/cogwxp
)
add_test(NAME cogwxp-tests COMMAND cogwxp-tests)
set_tests_properties(cogwxp-tests PROPERTIES
    TIMEOUT  60
    LABELS   "unit"
)

add_executable(test-niche-beast
    test_niche_beast.c
)
target_link_libraries(test-niche-beast
    cogwxp-cogutil
    cogwxp-atomspace
    cogwxp-pln
    Threads::Threads
    m
)
target_include_directories(test-niche-beast PRIVATE
    ${CMAKE_SOURCE_DIR}/cogwxp
)
add_test(NAME test-niche-beast COMMAND test-niche-beast)
set_tests_properties(test-niche-beast PROPERTIES
    TIMEOUT  120
    LABELS   "integration"
)

# ---------------------------------------------------------------------------
# New comprehensive unit tests
# ---------------------------------------------------------------------------

# PLN truth-value formula and inference-chain exhaustive tests
cogwxp_add_test(test-pln-formulas
    test_pln_formulas.c
    "unit"
    60
)

# CogUtil threading, string utils, data structures, timers
cogwxp_add_test(test-cogutil-advanced
    test_cogutil_advanced.c
    "unit"
    60
)

# AtomSpace query, spreading, child spaces, edge cases, serialization
cogwxp_add_test(test-atomspace-advanced
    test_atomspace_advanced.c
    "unit"
    60
)

# Null-safety and error-path tests for all core APIs
cogwxp_add_test(test-error-conditions
    test_error_conditions.c
    "unit"
    60
)

# ---------------------------------------------------------------------------
# Stress / load tests (labelled "stress" so they can be run separately)
# ---------------------------------------------------------------------------

cogwxp_add_test(test-stress
    test_stress.c
    "stress"
    600
)

# ---------------------------------------------------------------------------
# Orchestration layer tests (test_frameworks.c) — compiled only when the
# orchestration / atenspace / hypermind / cogzero targets are available.
# ---------------------------------------------------------------------------

if(TARGET cogwxp-orchestration OR TARGET atenspace OR TARGET orchestration)
    add_executable(test-frameworks
        test_frameworks.c
    )
    # Determine which orchestration target to link
    foreach(_orch_target cogwxp-orchestration atenspace orchestration)
        if(TARGET ${_orch_target})
            target_link_libraries(test-frameworks PRIVATE ${_orch_target})
        endif()
    endforeach()
    target_link_libraries(test-frameworks PRIVATE
        cogwxp-cogutil
        cogwxp-atomspace
        cogwxp-pln
        Threads::Threads
        m
    )
    target_include_directories(test-frameworks PRIVATE
        ${CMAKE_SOURCE_DIR}/cogwxp
    )
    add_test(NAME test-frameworks COMMAND test-frameworks)
    set_tests_properties(test-frameworks PROPERTIES
        TIMEOUT  120
        LABELS   "integration"
    )
endif()

# ---------------------------------------------------------------------------
# Convenience target: run everything
# ---------------------------------------------------------------------------

add_custom_target(run-tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --timeout 600
    DEPENDS
        cogwxp-tests
        test-niche-beast
        test-pln-formulas
        test-cogutil-advanced
        test-atomspace-advanced
        test-error-conditions
        test-stress
    COMMENT "Running full CoGWXP-OS9 test suite"
)

# Convenience target: unit tests only (fast)
add_custom_target(run-unit-tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L unit --timeout 120
    DEPENDS
        cogwxp-tests
        test-pln-formulas
        test-cogutil-advanced
        test-atomspace-advanced
        test-error-conditions
    COMMENT "Running CoGWXP-OS9 unit tests"
)

# Convenience target: stress tests only
add_custom_target(run-stress-tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L stress --timeout 600
    DEPENDS test-stress
    COMMENT "Running CoGWXP-OS9 stress tests"
)
