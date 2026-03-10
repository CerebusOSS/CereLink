# cbsdk - SDK Public API
# Orchestrates cbdev + cbshm to provide clean public C API

project(cbsdk
    DESCRIPTION "CereLink SDK"
    LANGUAGES CXX C
)

# Library sources
set(CBSDK_SOURCES
    src/sdk_session.cpp
    src/cbsdk.cpp
)

# Build as STATIC library
add_library(cbsdk STATIC ${CBSDK_SOURCES})

target_include_directories(cbsdk
    BEFORE PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Dependencies
target_link_libraries(cbsdk
    PUBLIC
        cbproto
        cbutil
        cbshm
        cbdev
    PRIVATE
        $<BUILD_INTERFACE:ccfutils>
)

# C++17 for implementation, C compatible API
target_compile_features(cbsdk PUBLIC cxx_std_17)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(cbsdk PRIVATE wsock32 ws2_32)
elseif(APPLE)
    target_link_libraries(cbsdk PRIVATE pthread)
else()
    target_link_libraries(cbsdk PRIVATE pthread)
endif()

# Shared library for FFI bindings (Python, C#, Matlab)
option(CBSDK_BUILD_SHARED "Build cbsdk as a shared library" OFF)
if(CBSDK_BUILD_SHARED)
    add_library(cbsdk_shared SHARED ${CBSDK_SOURCES})

    target_include_directories(cbsdk_shared
        BEFORE PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    target_link_libraries(cbsdk_shared
        PRIVATE
            cbproto cbutil cbshm cbdev ccfutils
    )

    target_compile_features(cbsdk_shared PUBLIC cxx_std_17)
    target_compile_definitions(cbsdk_shared PRIVATE CBSDK_SHARED CBSDK_EXPORTS)
    set_target_properties(cbsdk_shared PROPERTIES OUTPUT_NAME cbsdk)

    if(WIN32)
        target_link_libraries(cbsdk_shared PRIVATE wsock32 ws2_32)
    else()
        target_link_libraries(cbsdk_shared PRIVATE pthread)
    endif()
endif()

# Installation
install(TARGETS cbsdk
    EXPORT CBSDKTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# TODO: Install public headers when they exist
# install(
#     DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
#     DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
# )
