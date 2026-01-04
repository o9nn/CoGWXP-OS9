# CMakeLists.txt for CoGWXP-OS9 - Root Build Configuration
#
# CoGWXP-OS9: Cognitive Windows XP + Plan9 AGI Operating System
#
# This is the master build file that orchestrates the build of all components
# in the correct dependency order.

cmake_minimum_required(VERSION 3.16)
project(cogwxp-os9 
    VERSION 1.0.0 
    DESCRIPTION "Cognitive Windows XP + Plan9 AGI Operating System"
    LANGUAGES C CXX
)

# ===========================================================================
# Build Configuration
# ===========================================================================

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Default to Release build
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_DOCS "Build documentation" OFF)
option(ENABLE_9P_INTEGRATION "Enable Plan 9 9P protocol integration" ON)
option(ENABLE_DIS_INTEGRATION "Enable Inferno Dis VM integration" ON)
option(ENABLE_NT_INTEGRATION "Enable Windows NT kernel integration" OFF)
option(ENABLE_SSL "Enable SSL/TLS support" OFF)
option(ENABLE_JIT "Enable JIT compilation for Dis VM" OFF)

# ===========================================================================
# Output Directories
# ===========================================================================

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ===========================================================================
# Dependencies
# ===========================================================================

find_package(Threads REQUIRED)

if(ENABLE_SSL)
    find_package(OpenSSL)
endif()

find_package(Boost COMPONENTS system filesystem program_options)

# ===========================================================================
# Global Include Directories
# ===========================================================================

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# ===========================================================================
# Build Order (respecting dependencies)
# ===========================================================================

# 1. OpenCog Components (cogutil -> atomspace -> pln -> cogserver)
message(STATUS "Configuring OpenCog components...")
add_subdirectory(opencog)

# 2. Plan 9 Components (9P protocol)
if(ENABLE_9P_INTEGRATION)
    message(STATUS "Configuring Plan 9 components...")
    add_subdirectory(plan9)
endif()

# 3. Inferno Components (Dis VM, Limbo runtime, Styx)
if(ENABLE_DIS_INTEGRATION)
    message(STATUS "Configuring Inferno components...")
    add_subdirectory(inferno)
endif()

# 4. CogW7OS Cognitive Kernel
message(STATUS "Configuring CogW7OS cognitive kernel...")
add_subdirectory(cogw7os)

# 5. Integration Layer (unified bridge, CoGWXP-OS main library)
message(STATUS "Configuring Integration layer...")
add_subdirectory(integration)

# 6. Orchestration Layer (AGI toolchain orchestration)
message(STATUS "Configuring Orchestration layer...")
add_subdirectory(orchestration)

# 7. Test Suite
if(BUILD_TESTS)
    message(STATUS "Configuring Test suite...")
    add_subdirectory(tests)
endif()

# ===========================================================================
# Installation
# ===========================================================================

include(GNUInstallDirs)

# Install header files
install(DIRECTORY opencog/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cogwxp/opencog
    FILES_MATCHING PATTERN "*.h"
)

if(ENABLE_9P_INTEGRATION)
    install(DIRECTORY plan9/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cogwxp/plan9
        FILES_MATCHING PATTERN "*.h"
    )
endif()

if(ENABLE_DIS_INTEGRATION)
    install(DIRECTORY inferno/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cogwxp/inferno
        FILES_MATCHING PATTERN "*.h"
    )
endif()

install(DIRECTORY cogw7os/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cogwxp/cogw7os
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY integration/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cogwxp/integration
    FILES_MATCHING PATTERN "*.h"
)

# ===========================================================================
# Package Configuration
# ===========================================================================

include(CMakePackageConfigHelpers)

# Generate package config
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cogwxp-os9-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cogwxp-os9-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cogwxp-os9
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cogwxp-os9-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cogwxp-os9-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cogwxp-os9-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cogwxp-os9
)

# ===========================================================================
# pkg-config
# ===========================================================================

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cogwxp-os9.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/cogwxp-os9.pc
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cogwxp-os9.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# ===========================================================================
# Documentation
# ===========================================================================

if(BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in
            ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            @ONLY
        )
        add_custom_target(docs
            ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    endif()
endif()

# ===========================================================================
# Summary
# ===========================================================================

message(STATUS "")
message(STATUS "========================================")
message(STATUS "  CoGWXP-OS9 Configuration Summary")
message(STATUS "========================================")
message(STATUS "")
message(STATUS "  Version:            ${PROJECT_VERSION}")
message(STATUS "  Build type:         ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install prefix:     ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "  Components:")
message(STATUS "    OpenCog:          ON")
message(STATUS "    Plan 9 (9P):      ${ENABLE_9P_INTEGRATION}")
message(STATUS "    Inferno (Dis):    ${ENABLE_DIS_INTEGRATION}")
message(STATUS "    NT Integration:   ${ENABLE_NT_INTEGRATION}")
message(STATUS "")
message(STATUS "  Features:")
message(STATUS "    SSL/TLS:          ${ENABLE_SSL}")
message(STATUS "    JIT Compilation:  ${ENABLE_JIT}")
message(STATUS "")
message(STATUS "  Build options:")
message(STATUS "    Shared libs:      ${BUILD_SHARED_LIBS}")
message(STATUS "    Tests:            ${BUILD_TESTS}")
message(STATUS "    Examples:         ${BUILD_EXAMPLES}")
message(STATUS "    Documentation:    ${BUILD_DOCS}")
message(STATUS "")
message(STATUS "========================================")
message(STATUS "")
