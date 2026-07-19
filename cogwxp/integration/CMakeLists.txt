# CMakeLists.txt for CoGWXP-OS9 Integration Layer
# 
# Provides the unified bridge connecting all subsystems

cmake_minimum_required(VERSION 3.16)
project(cogwxp-integration VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(ENABLE_9P_INTEGRATION "Enable Plan 9 9P protocol integration" ON)
option(ENABLE_DIS_INTEGRATION "Enable Inferno Dis VM integration" ON)
option(ENABLE_NT_INTEGRATION "Enable Windows NT kernel integration" ON)

# Find dependencies
find_package(Threads REQUIRED)

# Find our own libraries
find_library(COGUTIL_LIB cogutil HINTS ${CMAKE_INSTALL_PREFIX}/lib)
find_library(ATOMSPACE_LIB atomspace HINTS ${CMAKE_INSTALL_PREFIX}/lib)
find_library(PLN_LIB pln HINTS ${CMAKE_INSTALL_PREFIX}/lib)
find_library(COGSERVER_LIB cogserver HINTS ${CMAKE_INSTALL_PREFIX}/lib)
find_library(COGW7OS_LIB cogw7os HINTS ${CMAKE_INSTALL_PREFIX}/lib)

if(ENABLE_9P_INTEGRATION)
    find_library(P9_LIB p9 HINTS ${CMAKE_INSTALL_PREFIX}/lib)
    find_library(ATOMSPACE_9P_LIB atomspace_9p HINTS ${CMAKE_INSTALL_PREFIX}/lib)
endif()

if(ENABLE_DIS_INTEGRATION)
    find_library(DIS_LIB dis HINTS ${CMAKE_INSTALL_PREFIX}/lib)
    find_library(LIMBO_RT_LIB limbo_rt HINTS ${CMAKE_INSTALL_PREFIX}/lib)
    find_library(STYX_LIB styx HINTS ${CMAKE_INSTALL_PREFIX}/lib)
    find_library(ATOMSPACE_DIS_LIB atomspace_dis HINTS ${CMAKE_INSTALL_PREFIX}/lib)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../opencog)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../plan9)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../inferno)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../cogw7os)
include_directories(${CMAKE_INSTALL_PREFIX}/include)

# ===========================================================================
# Unified Bridge Library
# ===========================================================================

set(BRIDGE_SOURCES
    bridges/unified_bridge.c
    bridges/b9_nodes.c
    bridges/p9_scopes.c
    bridges/j9_gradients.c
    bridges/resource_access.c
    bridges/event_system.c
    bridges/sync.c
)

set(BRIDGE_HEADERS
    bridges/unified_bridge.h
)

add_library(unified_bridge ${BRIDGE_SOURCES})
target_link_libraries(unified_bridge PUBLIC Threads::Threads)

# Link all available libraries
foreach(LIB cogutil atomspace pln cogserver cogw7os)
    if(TARGET ${LIB})
        target_link_libraries(unified_bridge PUBLIC ${LIB})
    endif()
endforeach()

if(ENABLE_9P_INTEGRATION)
    foreach(LIB p9 atomspace_9p)
        if(TARGET ${LIB})
            target_link_libraries(unified_bridge PUBLIC ${LIB})
        endif()
    endforeach()
    target_compile_definitions(unified_bridge PRIVATE ENABLE_9P_INTEGRATION=1)
endif()

if(ENABLE_DIS_INTEGRATION)
    foreach(LIB dis limbo_rt styx atomspace_dis)
        if(TARGET ${LIB})
            target_link_libraries(unified_bridge PUBLIC ${LIB})
        endif()
    endforeach()
    target_compile_definitions(unified_bridge PRIVATE ENABLE_DIS_INTEGRATION=1)
endif()

set_target_properties(unified_bridge PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${BRIDGE_HEADERS}"
)

# ===========================================================================
# CoGWXP-OS Main Library
# ===========================================================================

set(COGWXP_OS_SOURCES
    cogwxp_os.c
    cogwxp_init.c
    cogwxp_knowledge.c
    cogwxp_agents.c
    cogwxp_tasks.c
    cogwxp_distributed.c
    cogwxp_9p.c
    cogwxp_dis.c
    cogwxp_b9p9j9.c
    cogwxp_events.c
    cogwxp_stats.c
    niche_construction.c
    beast_mode.c
)

set(COGWXP_OS_HEADERS
    cogwxp_os.h
    niche_construction.h
    beast_mode.h
)

add_library(cogwxp_os ${COGWXP_OS_SOURCES})
target_link_libraries(cogwxp_os PUBLIC unified_bridge)

set_target_properties(cogwxp_os PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${COGWXP_OS_HEADERS}"
)

# ===========================================================================
# CoGWXP-OS Daemon
# ===========================================================================

add_executable(cogwxp-osd
    daemon/cogwxp_osd.c
    daemon/config.c
)
target_link_libraries(cogwxp-osd PRIVATE cogwxp_os)

# ===========================================================================
# CoGWXP-OS CLI
# ===========================================================================

add_executable(cogwxp
    cli/cogwxp.c
    cli/commands.c
    cli/repl.c
)
target_link_libraries(cogwxp PRIVATE cogwxp_os)

# ===========================================================================
# Examples
# ===========================================================================

if(BUILD_EXAMPLES)
    add_executable(example_hello examples/hello.c)
    target_link_libraries(example_hello PRIVATE cogwxp_os)
    
    add_executable(example_inference examples/inference.c)
    target_link_libraries(example_inference PRIVATE cogwxp_os)
    
    add_executable(example_agents examples/agents.c)
    target_link_libraries(example_agents PRIVATE cogwxp_os)
    
    add_executable(example_distributed examples/distributed.c)
    target_link_libraries(example_distributed PRIVATE cogwxp_os)
    
    add_executable(example_limbo examples/limbo.c)
    target_link_libraries(example_limbo PRIVATE cogwxp_os)
endif()

# ===========================================================================
# Installation
# ===========================================================================

include(GNUInstallDirs)

install(TARGETS unified_bridge cogwxp_os cogwxp-osd cogwxp
    EXPORT cogwxp-integration-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cogwxp
)

install(EXPORT cogwxp-integration-targets
    FILE cogwxp-integration-targets.cmake
    NAMESPACE cogwxp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cogwxp-integration
)

# ===========================================================================
# Tests
# ===========================================================================

if(BUILD_TESTS)
    enable_testing()
    
    add_executable(test_unified_bridge tests/test_unified_bridge.c)
    target_link_libraries(test_unified_bridge unified_bridge)
    add_test(NAME test_unified_bridge COMMAND test_unified_bridge)
    
    add_executable(test_cogwxp_os tests/test_cogwxp_os.c)
    target_link_libraries(test_cogwxp_os cogwxp_os)
    add_test(NAME test_cogwxp_os COMMAND test_cogwxp_os)
    
    add_executable(test_b9p9j9 tests/test_b9p9j9.c)
    target_link_libraries(test_b9p9j9 cogwxp_os)
    add_test(NAME test_b9p9j9 COMMAND test_b9p9j9)
endif()

# ===========================================================================
# Summary
# ===========================================================================

message(STATUS "")
message(STATUS "CoGWXP-OS9 Integration Layer Configuration:")
message(STATUS "  Version:          ${PROJECT_VERSION}")
message(STATUS "  9P Integration:   ${ENABLE_9P_INTEGRATION}")
message(STATUS "  Dis Integration:  ${ENABLE_DIS_INTEGRATION}")
message(STATUS "  NT Integration:   ${ENABLE_NT_INTEGRATION}")
message(STATUS "  Build tests:      ${BUILD_TESTS}")
message(STATUS "  Build examples:   ${BUILD_EXAMPLES}")
message(STATUS "")
