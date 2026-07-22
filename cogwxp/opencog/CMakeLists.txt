# CMakeLists.txt for CoGWXP-OS9 OpenCog Components
# 
# Build order:
# 1. cogutil (no dependencies)
# 2. atomspace (depends on cogutil)
# 3. pln (depends on atomspace)
# 4. cogserver (depends on pln)

cmake_minimum_required(VERSION 3.16)
project(cogwxp-opencog VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_TESTS "Build test suite" ON)
option(ENABLE_9P_INTEGRATION "Enable Plan 9 9P protocol integration" ON)
option(ENABLE_DIS_INTEGRATION "Enable Inferno Dis VM integration" ON)
option(ENABLE_NT_INTEGRATION "Enable Windows NT kernel integration" ON)

# Platform detection
if(WIN32)
    set(COGWXP_PLATFORM_NT ON)
    add_definitions(-DCOGUTIL_PLATFORM_NT=1)
elseif(UNIX)
    set(COGWXP_PLATFORM_POSIX ON)
    add_definitions(-DCOGUTIL_PLATFORM_POSIX=1)
endif()

# Find dependencies
find_package(Threads REQUIRED)

# Optional dependencies
find_package(Boost COMPONENTS system filesystem program_options)
find_package(PkgConfig)

if(PkgConfig_FOUND)
    pkg_check_modules(UUID uuid)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../plan9)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../inferno)

# ===========================================================================
# CogUtil Library
# ===========================================================================

set(COGUTIL_SOURCES
    cogutil/cogutil.c
    cogutil/logging.c
    cogutil/memory.c
    cogutil/threading.c
    cogutil/config.c
    cogutil/uuid.c
    cogutil/hash.c
    cogutil/timer.c
    cogutil/string_utils.c
    cogutil/array.c
    cogutil/hashmap.c
    cogutil/list.c
    cogutil/platform.c
)

set(COGUTIL_HEADERS
    cogutil/cogutil.h
)

add_library(cogutil ${COGUTIL_SOURCES})
target_link_libraries(cogutil PRIVATE Threads::Threads)

# Link to shared compile options for consistent warnings
if(TARGET cogwxp_compile_options)
    target_link_libraries(cogutil PRIVATE cogwxp_compile_options)
endif()

if(UUID_FOUND)
    target_include_directories(cogutil PRIVATE ${UUID_INCLUDE_DIRS})
    target_link_libraries(cogutil PRIVATE ${UUID_LIBRARIES})
endif()

if(WIN32)
    target_compile_definitions(cogutil PRIVATE COGUTIL_EXPORTS)
    target_link_libraries(cogutil PRIVATE ws2_32 kernel32)
endif()

set_target_properties(cogutil PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${COGUTIL_HEADERS}"
)

# ===========================================================================
# AtomSpace Library
# ===========================================================================

set(ATOMSPACE_SOURCES
    atomspace/atomspace.c
    atomspace/atom.c
    atomspace/truth_value.c
    atomspace/attention_value.c
    atomspace/query.c
    atomspace/pattern_match.c
    atomspace/attention_bank.c
    atomspace/spreading.c
    atomspace/serialization.c
)

if(ENABLE_9P_INTEGRATION)
    list(APPEND ATOMSPACE_SOURCES atomspace/atomspace_9p.c)
endif()

if(ENABLE_DIS_INTEGRATION)
    list(APPEND ATOMSPACE_SOURCES atomspace/atomspace_dis.c)
endif()

set(ATOMSPACE_HEADERS
    atomspace/atomspace.h
)

add_library(atomspace ${ATOMSPACE_SOURCES})
target_link_libraries(atomspace PUBLIC cogutil)

# Link to shared compile options for consistent warnings
if(TARGET cogwxp_compile_options)
    target_link_libraries(atomspace PRIVATE cogwxp_compile_options)
endif()

if(ENABLE_9P_INTEGRATION)
    target_link_libraries(atomspace PRIVATE p9)
    target_compile_definitions(atomspace PRIVATE ENABLE_9P_INTEGRATION=1)
endif()

if(ENABLE_DIS_INTEGRATION)
    target_link_libraries(atomspace PRIVATE dis)
    target_compile_definitions(atomspace PRIVATE ENABLE_DIS_INTEGRATION=1)
endif()

if(WIN32)
    target_compile_definitions(atomspace PRIVATE ATOMSPACE_EXPORTS)
endif()

set_target_properties(atomspace PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${ATOMSPACE_HEADERS}"
)

# ===========================================================================
# PLN Library
# ===========================================================================

set(PLN_SOURCES
    pln/pln.c
    pln/forward_chain.c
    pln/backward_chain.c
    pln/rules.c
    pln/truth_value_formulas.c
    pln/inference_control.c
    pln/inference_history.c
    pln/distributed_pln.c
)

set(PLN_HEADERS
    pln/pln.h
)

add_library(pln ${PLN_SOURCES})
target_link_libraries(pln PUBLIC atomspace)
if(NOT WIN32)
    target_link_libraries(pln PRIVATE m)
endif()

# Link to shared compile options for consistent warnings
if(TARGET cogwxp_compile_options)
    target_link_libraries(pln PRIVATE cogwxp_compile_options)
endif()

if(ENABLE_9P_INTEGRATION)
    target_compile_definitions(pln PRIVATE ENABLE_9P_INTEGRATION=1)
endif()

if(ENABLE_DIS_INTEGRATION)
    target_compile_definitions(pln PRIVATE ENABLE_DIS_INTEGRATION=1)
endif()

if(WIN32)
    target_compile_definitions(pln PRIVATE PLN_EXPORTS)
endif()

set_target_properties(pln PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${PLN_HEADERS}"
)

# ===========================================================================
# CogServer Library
# ===========================================================================

set(COGSERVER_SOURCES
    cogserver/cogserver.c
    cogserver/agent.c
    cogserver/orchestrator.c
    cogserver/task_scheduler.c
    cogserver/network.c
)

set(COGSERVER_HEADERS
    cogserver/cogserver.h
)

add_library(cogserver ${COGSERVER_SOURCES})
target_link_libraries(cogserver PUBLIC pln)

# Link to shared compile options for consistent warnings
if(TARGET cogwxp_compile_options)
    target_link_libraries(cogserver PRIVATE cogwxp_compile_options)
endif()

if(WIN32)
    target_compile_definitions(cogserver PRIVATE COGSERVER_EXPORTS)
endif()

set_target_properties(cogserver PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${COGSERVER_HEADERS}"
)

# ===========================================================================
# Installation
# ===========================================================================

include(GNUInstallDirs)

install(TARGETS cogutil atomspace pln cogserver
    EXPORT cogwxp-opencog-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cogwxp
)

install(EXPORT cogwxp-opencog-targets
    FILE cogwxp-opencog-targets.cmake
    NAMESPACE cogwxp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cogwxp-opencog
)

# ===========================================================================
# Package Configuration
# ===========================================================================

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cogwxp-opencog-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cogwxp-opencog-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cogwxp-opencog
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cogwxp-opencog-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cogwxp-opencog-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cogwxp-opencog-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cogwxp-opencog
)

# ===========================================================================
# Tests
# ===========================================================================

if(BUILD_TESTS)
    enable_testing()
    
    add_executable(test_cogutil tests/test_cogutil.c)
    target_link_libraries(test_cogutil cogutil)
    add_test(NAME test_cogutil COMMAND test_cogutil)
    
    add_executable(test_atomspace tests/test_atomspace.c)
    target_link_libraries(test_atomspace atomspace)
    add_test(NAME test_atomspace COMMAND test_atomspace)
    
    add_executable(test_pln tests/test_pln.c)
    target_link_libraries(test_pln pln)
    add_test(NAME test_pln COMMAND test_pln)
endif()

# ===========================================================================
# Summary
# ===========================================================================

message(STATUS "")
message(STATUS "CoGWXP-OS9 OpenCog Components Configuration:")
message(STATUS "  Version:              ${PROJECT_VERSION}")
message(STATUS "  Platform:             ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Build type:           ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared libraries:     ${BUILD_SHARED_LIBS}")
message(STATUS "  9P Integration:       ${ENABLE_9P_INTEGRATION}")
message(STATUS "  Dis VM Integration:   ${ENABLE_DIS_INTEGRATION}")
message(STATUS "  NT Integration:       ${ENABLE_NT_INTEGRATION}")
message(STATUS "  Build tests:          ${BUILD_TESTS}")
message(STATUS "")
