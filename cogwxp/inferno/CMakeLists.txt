# CMakeLists.txt for CoGWXP-OS9 Inferno Components
# 
# Provides Dis virtual machine for Limbo bytecode execution

cmake_minimum_required(VERSION 3.16)
project(cogwxp-inferno VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_TESTS "Build test suite" ON)
option(ENABLE_JIT "Enable JIT compilation" OFF)

# Find dependencies
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../opencog/cogutil)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../plan9)

# ===========================================================================
# Dis Virtual Machine Library
# ===========================================================================

set(DIS_SOURCES
    dis/dis.c
    dis/dis_vm.c
    dis/dis_thread.c
    dis/dis_module.c
    dis/dis_channel.c
    dis/dis_gc.c
    dis/dis_array.c
    dis/dis_value.c
    dis/dis_interp.c
    dis/dis_debug.c
)

if(ENABLE_JIT)
    list(APPEND DIS_SOURCES dis/dis_jit.c)
endif()

set(DIS_HEADERS
    dis/dis.h
)

add_library(dis ${DIS_SOURCES})
target_link_libraries(dis PRIVATE Threads::Threads)

if(ENABLE_JIT)
    target_compile_definitions(dis PRIVATE ENABLE_JIT=1)
endif()

set_target_properties(dis PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${DIS_HEADERS}"
)

# ===========================================================================
# Limbo Runtime Library
# ===========================================================================

set(LIMBO_RT_SOURCES
    limbo/runtime/sys.c
    limbo/runtime/math.c
    limbo/runtime/string.c
    limbo/runtime/draw.c
    limbo/runtime/bufio.c
)

add_library(limbo_rt ${LIMBO_RT_SOURCES})
target_link_libraries(limbo_rt PUBLIC dis)

# ===========================================================================
# Dis Interpreter (emu)
# ===========================================================================

add_executable(emu
    tools/emu.c
)
target_link_libraries(emu PRIVATE dis limbo_rt)

# ===========================================================================
# AtomSpace Dis Integration
# ===========================================================================

set(ATOMSPACE_DIS_SOURCES
    atomspace_dis/atomspace_dis.c
    atomspace_dis/atomspace_dis_types.c
)

add_library(atomspace_dis ${ATOMSPACE_DIS_SOURCES})
target_link_libraries(atomspace_dis PUBLIC dis)

# ===========================================================================
# Styx Protocol (9P over Dis channels)
# ===========================================================================

set(STYX_SOURCES
    styx/styx.c
    styx/styx_client.c
    styx/styx_server.c
)

add_library(styx ${STYX_SOURCES})
target_link_libraries(styx PUBLIC dis)

# ===========================================================================
# Installation
# ===========================================================================

include(GNUInstallDirs)

install(TARGETS dis limbo_rt atomspace_dis styx emu
    EXPORT cogwxp-inferno-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cogwxp/inferno
)

install(EXPORT cogwxp-inferno-targets
    FILE cogwxp-inferno-targets.cmake
    NAMESPACE cogwxp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cogwxp-inferno
)

# ===========================================================================
# Tests
# ===========================================================================

if(BUILD_TESTS)
    enable_testing()
    
    add_executable(test_dis tests/test_dis.c)
    target_link_libraries(test_dis dis)
    add_test(NAME test_dis COMMAND test_dis)
    
    add_executable(test_dis_channel tests/test_dis_channel.c)
    target_link_libraries(test_dis_channel dis)
    add_test(NAME test_dis_channel COMMAND test_dis_channel)
    
    add_executable(test_styx tests/test_styx.c)
    target_link_libraries(test_styx styx)
    add_test(NAME test_styx COMMAND test_styx)
endif()

# ===========================================================================
# Summary
# ===========================================================================

message(STATUS "")
message(STATUS "CoGWXP-OS9 Inferno Components Configuration:")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  JIT Support:    ${ENABLE_JIT}")
message(STATUS "  Build tests:    ${BUILD_TESTS}")
message(STATUS "")
