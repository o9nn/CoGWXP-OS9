# CMakeLists.txt for CoGWXP-OS9 CogW7OS Cognitive Kernel
# 
# The cognitive kernel extends NT with OpenCog, 9P, and Dis integration

cmake_minimum_required(VERSION 3.16)
project(cogwxp-cogw7os VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_TESTS "Build test suite" ON)
option(ENABLE_9P_INTEGRATION "Enable Plan 9 9P protocol integration" ON)
option(ENABLE_DIS_INTEGRATION "Enable Inferno Dis VM integration" ON)
option(ENABLE_NT_INTEGRATION "Enable Windows NT kernel integration" ON)

# Find dependencies
find_package(Threads REQUIRED)

# Find our own libraries
find_library(COGUTIL_LIB cogutil HINTS ${CMAKE_INSTALL_PREFIX}/lib)
find_library(ATOMSPACE_LIB atomspace HINTS ${CMAKE_INSTALL_PREFIX}/lib)
find_library(PLN_LIB pln HINTS ${CMAKE_INSTALL_PREFIX}/lib)

if(ENABLE_9P_INTEGRATION)
    find_library(P9_LIB p9 HINTS ${CMAKE_INSTALL_PREFIX}/lib)
endif()

if(ENABLE_DIS_INTEGRATION)
    find_library(DIS_LIB dis HINTS ${CMAKE_INSTALL_PREFIX}/lib)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../opencog)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../plan9)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../inferno)
include_directories(${CMAKE_INSTALL_PREFIX}/include)

# ===========================================================================
# CogW7OS Kernel Library
# ===========================================================================

set(COGW7OS_SOURCES
    kernel/cogw7os.c
    kernel/process.c
    kernel/thread.c
    kernel/agent.c
    kernel/task.c
    kernel/scheduler.c
    kernel/memory.c
    kernel/ipc.c
    kernel/handle.c
)

if(ENABLE_9P_INTEGRATION)
    list(APPEND COGW7OS_SOURCES kernel/9p_integration.c)
endif()

if(ENABLE_DIS_INTEGRATION)
    list(APPEND COGW7OS_SOURCES kernel/dis_integration.c)
endif()

if(ENABLE_NT_INTEGRATION)
    list(APPEND COGW7OS_SOURCES kernel/nt_bridge.c)
endif()

set(COGW7OS_HEADERS
    kernel/cogw7os.h
)

add_library(cogw7os ${COGW7OS_SOURCES})
target_link_libraries(cogw7os PUBLIC Threads::Threads)

if(TARGET cogutil)
    target_link_libraries(cogw7os PUBLIC cogutil)
endif()

if(TARGET atomspace)
    target_link_libraries(cogw7os PUBLIC atomspace)
endif()

if(TARGET pln)
    target_link_libraries(cogw7os PUBLIC pln)
endif()

if(ENABLE_9P_INTEGRATION AND TARGET p9)
    target_link_libraries(cogw7os PUBLIC p9)
    target_compile_definitions(cogw7os PRIVATE ENABLE_9P_INTEGRATION=1)
endif()

if(ENABLE_DIS_INTEGRATION AND TARGET dis)
    target_link_libraries(cogw7os PUBLIC dis)
    target_compile_definitions(cogw7os PRIVATE ENABLE_DIS_INTEGRATION=1)
endif()

if(ENABLE_NT_INTEGRATION)
    target_compile_definitions(cogw7os PRIVATE ENABLE_NT_INTEGRATION=1)
endif()

if(WIN32)
    target_compile_definitions(cogw7os PRIVATE COGW7OS_EXPORTS)
endif()

set_target_properties(cogw7os PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${COGW7OS_HEADERS}"
)

# ===========================================================================
# CogW7OS Daemon
# ===========================================================================

add_executable(cogw7osd
    daemon/cogw7osd.c
    daemon/config.c
    daemon/service.c
)
target_link_libraries(cogw7osd PRIVATE cogw7os)

# ===========================================================================
# CogW7OS CLI
# ===========================================================================

add_executable(cogw7ctl
    cli/cogw7ctl.c
    cli/commands.c
)
target_link_libraries(cogw7ctl PRIVATE cogw7os)

# ===========================================================================
# Installation
# ===========================================================================

include(GNUInstallDirs)

install(TARGETS cogw7os cogw7osd cogw7ctl
    EXPORT cogwxp-cogw7os-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cogwxp/cogw7os
)

install(EXPORT cogwxp-cogw7os-targets
    FILE cogwxp-cogw7os-targets.cmake
    NAMESPACE cogwxp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cogwxp-cogw7os
)

# ===========================================================================
# Tests
# ===========================================================================

if(BUILD_TESTS)
    enable_testing()
    
    add_executable(test_cogw7os tests/test_cogw7os.c)
    target_link_libraries(test_cogw7os cogw7os)
    add_test(NAME test_cogw7os COMMAND test_cogw7os)
    
    add_executable(test_agent tests/test_agent.c)
    target_link_libraries(test_agent cogw7os)
    add_test(NAME test_agent COMMAND test_agent)
    
    add_executable(test_scheduler tests/test_scheduler.c)
    target_link_libraries(test_scheduler cogw7os)
    add_test(NAME test_scheduler COMMAND test_scheduler)
endif()

# ===========================================================================
# Summary
# ===========================================================================

message(STATUS "")
message(STATUS "CoGWXP-OS9 CogW7OS Configuration:")
message(STATUS "  Version:          ${PROJECT_VERSION}")
message(STATUS "  9P Integration:   ${ENABLE_9P_INTEGRATION}")
message(STATUS "  Dis Integration:  ${ENABLE_DIS_INTEGRATION}")
message(STATUS "  NT Integration:   ${ENABLE_NT_INTEGRATION}")
message(STATUS "  Build tests:      ${BUILD_TESTS}")
message(STATUS "")
