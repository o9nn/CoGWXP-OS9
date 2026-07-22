# OSAL - Operating System Abstraction Layer
cmake_minimum_required(VERSION 3.16)

set(OSAL_SOURCES
    osal.c
)

set(OSAL_HEADERS
    osal.h
)

# Build as shared library
add_library(osal SHARED ${OSAL_SOURCES} ${OSAL_HEADERS})

# Set include directories
target_include_directories(osal
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# Windows DLL export settings
if(WIN32)
    target_compile_definitions(osal PRIVATE OSAL_EXPORTS)
    set_target_properties(osal PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
endif()

# Platform-specific link libraries
if(WIN32)
    target_link_libraries(osal PRIVATE advapi32)
else()
    target_link_libraries(osal PRIVATE pthread dl)
endif()

# Apply compile options from parent if available
if(TARGET cogwxp_compile_options)
    target_link_libraries(osal PRIVATE cogwxp_compile_options)
endif()

# Set output name
set_target_properties(osal PROPERTIES
    OUTPUT_NAME "cogwxp_osal"
    VERSION 1.0.0
    SOVERSION 1
)

# Install rules
install(TARGETS osal
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ${OSAL_HEADERS}
    DESTINATION include/cogwxp/osal
)
