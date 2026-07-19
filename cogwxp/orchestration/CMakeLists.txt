# CMakeLists.txt for CoGWXP-OS9 Orchestration Layer
#
# Unified build system for AGI toolchain orchestration

cmake_minimum_required(VERSION 3.16)
project(cogwxp-orchestration VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ===========================================================================
# Options
# ===========================================================================

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build example programs" ON)

# Component options
option(ENABLE_MSGRAPH "Enable MS Graph integration" ON)
option(ENABLE_COGPILOT "Enable CogPilot (HyperAgentQL, AzureCogML, PNN)" ON)
option(ENABLE_OSDEPLOY "Enable OSDeploy toolchain" ON)
option(ENABLE_AZURITE "Enable Azurite cognitive architecture" ON)
option(ENABLE_TOOLCHAINS "Enable additional toolchains (Torch7u, DynaV, etc.)" ON)

# ===========================================================================
# Dependencies
# ===========================================================================

find_package(Threads REQUIRED)
find_package(CURL)
find_package(OpenSSL)

# Find our own libraries
find_library(COGUTIL_LIB cogutil HINTS ${CMAKE_INSTALL_PREFIX}/lib)
find_library(ATOMSPACE_LIB atomspace HINTS ${CMAKE_INSTALL_PREFIX}/lib)
find_library(PLN_LIB pln HINTS ${CMAKE_INSTALL_PREFIX}/lib)
find_library(COGSERVER_LIB cogserver HINTS ${CMAKE_INSTALL_PREFIX}/lib)
find_library(P9_LIB p9 HINTS ${CMAKE_INSTALL_PREFIX}/lib)
find_library(DIS_LIB dis HINTS ${CMAKE_INSTALL_PREFIX}/lib)

# ===========================================================================
# Include directories
# ===========================================================================

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../opencog)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../plan9)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../inferno)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../cogw7os)
include_directories(${CMAKE_INSTALL_PREFIX}/include)

# ===========================================================================
# MS Graph Integration
# ===========================================================================

if(ENABLE_MSGRAPH)
    set(MSGRAPH_SOURCES
        msgraph/atomspace/ms_hypergraph.c
        msgraph/filesystem/mshypergraph_fs.c
        msgraph/queries/hypergraphiql.c
        msgraph/queries/hypergraphiql_parser.c
        msgraph/queries/hypergraphiql_executor.c
    )
    
    set(MSGRAPH_HEADERS
        msgraph/atomspace/ms_hypergraph.h
        msgraph/filesystem/mshypergraph_fs.h
        msgraph/queries/hypergraphiql.h
    )
    
    add_library(msgraph ${MSGRAPH_SOURCES})
    target_link_libraries(msgraph PUBLIC Threads::Threads)
    
    if(CURL_FOUND)
        target_link_libraries(msgraph PUBLIC CURL::libcurl)
    endif()
    
    if(COGUTIL_LIB)
        target_link_libraries(msgraph PUBLIC ${COGUTIL_LIB})
    endif()
    
    if(ATOMSPACE_LIB)
        target_link_libraries(msgraph PUBLIC ${ATOMSPACE_LIB})
    endif()
    
    if(P9_LIB)
        target_link_libraries(msgraph PUBLIC ${P9_LIB})
    endif()
    
    set_target_properties(msgraph PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
        PUBLIC_HEADER "${MSGRAPH_HEADERS}"
    )
endif()

# ===========================================================================
# CogPilot Integration
# ===========================================================================

if(ENABLE_COGPILOT)
    set(COGPILOT_SOURCES
        cogpilot/cogpilot.c
        cogpilot/hyperagentql.c
        cogpilot/azure_cogml.c
        cogpilot/planetary_neural_net.c
    )
    
    set(COGPILOT_HEADERS
        cogpilot/cogpilot.h
    )
    
    add_library(cogpilot ${COGPILOT_SOURCES})
    target_link_libraries(cogpilot PUBLIC Threads::Threads)
    
    if(CURL_FOUND)
        target_link_libraries(cogpilot PUBLIC CURL::libcurl)
    endif()
    
    if(COGUTIL_LIB)
        target_link_libraries(cogpilot PUBLIC ${COGUTIL_LIB})
    endif()
    
    if(ATOMSPACE_LIB)
        target_link_libraries(cogpilot PUBLIC ${ATOMSPACE_LIB})
    endif()
    
    if(COGSERVER_LIB)
        target_link_libraries(cogpilot PUBLIC ${COGSERVER_LIB})
    endif()
    
    if(ENABLE_MSGRAPH)
        target_link_libraries(cogpilot PUBLIC msgraph)
    endif()
    
    set_target_properties(cogpilot PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
        PUBLIC_HEADER "${COGPILOT_HEADERS}"
    )
endif()

# ===========================================================================
# OSDeploy Integration
# ===========================================================================

if(ENABLE_OSDEPLOY)
    set(OSDEPLOY_SOURCES
        osdeploy/osdeploy.c
        osdeploy/osdcloud.c
        osdeploy/driver_packs.c
        osdeploy/azure_deploy.c
        osdeploy/agent_deploy.c
        osdeploy/powershell.c
    )
    
    set(OSDEPLOY_HEADERS
        osdeploy/osdeploy.h
    )
    
    add_library(osdeploy ${OSDEPLOY_SOURCES})
    target_link_libraries(osdeploy PUBLIC Threads::Threads)
    
    if(COGUTIL_LIB)
        target_link_libraries(osdeploy PUBLIC ${COGUTIL_LIB})
    endif()
    
    if(ATOMSPACE_LIB)
        target_link_libraries(osdeploy PUBLIC ${ATOMSPACE_LIB})
    endif()
    
    if(ENABLE_COGPILOT)
        target_link_libraries(osdeploy PUBLIC cogpilot)
    endif()
    
    set_target_properties(osdeploy PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
        PUBLIC_HEADER "${OSDEPLOY_HEADERS}"
    )
endif()

# ===========================================================================
# Azurite Cognitive Architecture
# ===========================================================================

if(ENABLE_AZURITE)
    set(AZURITE_SOURCES
        azurite/azurite_cognitive.c
        azurite/memory_stream.c
        azurite/reflection.c
        azurite/planning.c
        azurite/reaction.c
        azurite/emotion.c
        azurite/relationships.c
    )
    
    set(AZURITE_HEADERS
        azurite/azurite_cognitive.h
    )
    
    add_library(azurite ${AZURITE_SOURCES})
    target_link_libraries(azurite PUBLIC Threads::Threads)
    
    if(COGUTIL_LIB)
        target_link_libraries(azurite PUBLIC ${COGUTIL_LIB})
    endif()
    
    if(ATOMSPACE_LIB)
        target_link_libraries(azurite PUBLIC ${ATOMSPACE_LIB})
    endif()
    
    if(ENABLE_COGPILOT)
        target_link_libraries(azurite PUBLIC cogpilot)
    endif()
    
    set_target_properties(azurite PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
        PUBLIC_HEADER "${AZURITE_HEADERS}"
    )
endif()

# ===========================================================================
# Additional Toolchains
# ===========================================================================

if(ENABLE_TOOLCHAINS)
    set(TOOLCHAINS_SOURCES
        toolchains/toolchains.c
        toolchains/torch7u.c
        toolchains/dynav.c
        toolchains/azstahcog.c
        toolchains/psforgithub.c
        toolchains/hypermind.c
    )
    
    set(TOOLCHAINS_HEADERS
        toolchains/toolchains.h
    )
    
    add_library(toolchains ${TOOLCHAINS_SOURCES})
    target_link_libraries(toolchains PUBLIC Threads::Threads)
    
    if(COGUTIL_LIB)
        target_link_libraries(toolchains PUBLIC ${COGUTIL_LIB})
    endif()
    
    if(ATOMSPACE_LIB)
        target_link_libraries(toolchains PUBLIC ${ATOMSPACE_LIB})
    endif()
    
    if(ENABLE_COGPILOT)
        target_link_libraries(toolchains PUBLIC cogpilot)
    endif()
    
    set_target_properties(toolchains PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
        PUBLIC_HEADER "${TOOLCHAINS_HEADERS}"
    )
endif()

# ===========================================================================
# ATenSpace / CogZero / HyperMind Frameworks
# ===========================================================================

set(ATENSPACE_SOURCES
    atenspace/atenspace.c
    cogzero/cogzero.c
    hypermind/hypermind.c
)

set(ATENSPACE_HEADERS
    atenspace/atenspace.h
    cogzero/cogzero.h
    hypermind/hypermind.h
)

add_library(atenspace ${ATENSPACE_SOURCES})
target_link_libraries(atenspace PUBLIC Threads::Threads m)

if(TARGET cogutil)
    target_link_libraries(atenspace PUBLIC cogutil)
elseif(COGUTIL_LIB)
    target_link_libraries(atenspace PUBLIC ${COGUTIL_LIB})
endif()

if(TARGET atomspace)
    target_link_libraries(atenspace PUBLIC atomspace)
elseif(ATOMSPACE_LIB)
    target_link_libraries(atenspace PUBLIC ${ATOMSPACE_LIB})
endif()

if(TARGET pln)
    target_link_libraries(atenspace PUBLIC pln)
elseif(PLN_LIB)
    target_link_libraries(atenspace PUBLIC ${PLN_LIB})
endif()

set_target_properties(atenspace PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${ATENSPACE_HEADERS}"
)

# ===========================================================================
# Unified Orchestration Library
# ===========================================================================

set(ORCHESTRATION_SOURCES
    orchestration.c
)

set(ORCHESTRATION_HEADERS
    orchestration.h
)

add_library(orchestration ${ORCHESTRATION_SOURCES})
target_link_libraries(orchestration PUBLIC Threads::Threads)

if(ENABLE_MSGRAPH)
    target_link_libraries(orchestration PUBLIC msgraph)
    target_compile_definitions(orchestration PRIVATE ENABLE_MSGRAPH=1)
endif()

if(ENABLE_COGPILOT)
    target_link_libraries(orchestration PUBLIC cogpilot)
    target_compile_definitions(orchestration PRIVATE ENABLE_COGPILOT=1)
endif()

if(ENABLE_OSDEPLOY)
    target_link_libraries(orchestration PUBLIC osdeploy)
    target_compile_definitions(orchestration PRIVATE ENABLE_OSDEPLOY=1)
endif()

if(ENABLE_AZURITE)
    target_link_libraries(orchestration PUBLIC azurite)
    target_compile_definitions(orchestration PRIVATE ENABLE_AZURITE=1)
endif()

if(ENABLE_TOOLCHAINS)
    target_link_libraries(orchestration PUBLIC toolchains)
    target_compile_definitions(orchestration PRIVATE ENABLE_TOOLCHAINS=1)
endif()

set_target_properties(orchestration PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${ORCHESTRATION_HEADERS}"
)

# ===========================================================================
# Orchestration CLI
# ===========================================================================

add_executable(cogwxp-orchestrate
    cli/orchestrate.c
    cli/commands.c
)
target_link_libraries(cogwxp-orchestrate PRIVATE orchestration)

# ===========================================================================
# Installation
# ===========================================================================

include(GNUInstallDirs)

set(INSTALL_TARGETS orchestration cogwxp-orchestrate)
list(APPEND INSTALL_TARGETS atenspace)

if(ENABLE_MSGRAPH)
    list(APPEND INSTALL_TARGETS msgraph)
endif()

if(ENABLE_COGPILOT)
    list(APPEND INSTALL_TARGETS cogpilot)
endif()

if(ENABLE_OSDEPLOY)
    list(APPEND INSTALL_TARGETS osdeploy)
endif()

if(ENABLE_AZURITE)
    list(APPEND INSTALL_TARGETS azurite)
endif()

if(ENABLE_TOOLCHAINS)
    list(APPEND INSTALL_TARGETS toolchains)
endif()

install(TARGETS ${INSTALL_TARGETS}
    EXPORT cogwxp-orchestration-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cogwxp/orchestration
)

install(EXPORT cogwxp-orchestration-targets
    FILE cogwxp-orchestration-targets.cmake
    NAMESPACE cogwxp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cogwxp-orchestration
)

# ===========================================================================
# Tests
# ===========================================================================

if(BUILD_TESTS)
    enable_testing()
    
    if(ENABLE_MSGRAPH)
        add_executable(test_msgraph tests/test_msgraph.c)
        target_link_libraries(test_msgraph msgraph)
        add_test(NAME test_msgraph COMMAND test_msgraph)
        
        add_executable(test_hypergraphiql tests/test_hypergraphiql.c)
        target_link_libraries(test_hypergraphiql msgraph)
        add_test(NAME test_hypergraphiql COMMAND test_hypergraphiql)
    endif()
    
    if(ENABLE_COGPILOT)
        add_executable(test_cogpilot tests/test_cogpilot.c)
        target_link_libraries(test_cogpilot cogpilot)
        add_test(NAME test_cogpilot COMMAND test_cogpilot)
    endif()
    
    if(ENABLE_AZURITE)
        add_executable(test_azurite tests/test_azurite.c)
        target_link_libraries(test_azurite azurite)
        add_test(NAME test_azurite COMMAND test_azurite)
    endif()
    
    add_executable(test_orchestration tests/test_orchestration.c)
    target_link_libraries(test_orchestration orchestration)
    add_test(NAME test_orchestration COMMAND test_orchestration)
endif()

# ===========================================================================
# Summary
# ===========================================================================

message(STATUS "")
message(STATUS "CoGWXP-OS9 Orchestration Configuration:")
message(STATUS "  Version:          ${PROJECT_VERSION}")
message(STATUS "  MS Graph:         ${ENABLE_MSGRAPH}")
message(STATUS "  CogPilot:         ${ENABLE_COGPILOT}")
message(STATUS "  OSDeploy:         ${ENABLE_OSDEPLOY}")
message(STATUS "  Azurite:          ${ENABLE_AZURITE}")
message(STATUS "  Toolchains:       ${ENABLE_TOOLCHAINS}")
message(STATUS "  Build tests:      ${BUILD_TESTS}")
message(STATUS "  Build examples:   ${BUILD_EXAMPLES}")
message(STATUS "")
