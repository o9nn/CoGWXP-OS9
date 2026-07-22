# CMakeLists.txt for CoGWXP-OS9 Plan 9 Components
# 
# Provides 9P protocol implementation for distributed file system access

cmake_minimum_required(VERSION 3.16)
project(cogwxp-plan9 VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_TESTS "Build test suite" ON)
option(ENABLE_SSL "Enable SSL/TLS support" OFF)

# Find dependencies
find_package(Threads REQUIRED)

if(ENABLE_SSL)
    find_package(OpenSSL REQUIRED)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../opencog/cogutil)

# ===========================================================================
# 9P Protocol Library
# ===========================================================================

set(P9_SOURCES
    9p/9p.c
    9p/9p_message.c
    9p/9p_server.c
    9p/9p_client.c
    9p/9p_namespace.c
    9p/9p_file.c
    9p/9p_auth.c
    9p/9p_transport.c
)

set(P9_HEADERS
    9p/9p.h
)

add_library(p9 ${P9_SOURCES})
target_link_libraries(p9 PRIVATE Threads::Threads)
if(TARGET cogutil)
    target_link_libraries(p9 PUBLIC cogutil)
endif()

# Link to shared compile options for consistent warnings
if(TARGET cogwxp_compile_options)
    target_link_libraries(p9 PRIVATE cogwxp_compile_options)
endif()

if(ENABLE_SSL)
    target_link_libraries(p9 PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(p9 PRIVATE ENABLE_SSL=1)
endif()

if(WIN32)
    target_link_libraries(p9 PRIVATE ws2_32)
endif()

set_target_properties(p9 PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${P9_HEADERS}"
)

# ===========================================================================
# 9P File Server (9fs)
# ===========================================================================

add_executable(9fs
    tools/9fs.c
)
target_link_libraries(9fs PRIVATE p9)

# ===========================================================================
# 9P Client Tool (9p)
# ===========================================================================

add_executable(9pclient
    tools/9pclient.c
)
target_link_libraries(9pclient PRIVATE p9)

# ===========================================================================
# AtomSpace 9P Export
# ===========================================================================

set(ATOMSPACE_9P_SOURCES
    atomspace_9p/atomspace_9p.c
    atomspace_9p/atomspace_9p_server.c
)

add_library(atomspace_9p ${ATOMSPACE_9P_SOURCES})
target_link_libraries(atomspace_9p PUBLIC p9)

# ===========================================================================
# Installation
# ===========================================================================

include(GNUInstallDirs)

install(TARGETS p9 atomspace_9p 9fs 9pclient
    EXPORT cogwxp-plan9-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cogwxp/plan9
)

install(EXPORT cogwxp-plan9-targets
    FILE cogwxp-plan9-targets.cmake
    NAMESPACE cogwxp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cogwxp-plan9
)

# ===========================================================================
# Tests
# ===========================================================================

if(BUILD_TESTS)
    enable_testing()
    
    add_executable(test_9p tests/test_9p.c)
    target_link_libraries(test_9p p9)
    add_test(NAME test_9p COMMAND test_9p)
    
    add_executable(test_9p_namespace tests/test_9p_namespace.c)
    target_link_libraries(test_9p_namespace p9)
    add_test(NAME test_9p_namespace COMMAND test_9p_namespace)
endif()

# ===========================================================================
# Summary
# ===========================================================================

message(STATUS "")
message(STATUS "CoGWXP-OS9 Plan 9 Components Configuration:")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  SSL Support:    ${ENABLE_SSL}")
message(STATUS "  Build tests:    ${BUILD_TESTS}")
message(STATUS "")
