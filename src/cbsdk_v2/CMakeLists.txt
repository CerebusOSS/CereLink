# cbsdk_v2 - SDK Public API
# Orchestrates cbdev + cbshmem to provide clean public C API

project(cbsdk_v2
    DESCRIPTION "CereLink SDK v2"
    LANGUAGES CXX C
)

# Library sources
set(CBSDK_V2_SOURCES
    src/sdk_session.cpp
    src/cbsdk.cpp
)

# Build as STATIC library
add_library(cbsdk_v2 STATIC ${CBSDK_V2_SOURCES})

target_include_directories(cbsdk_v2
    BEFORE PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        # upstream needed for protocol definitions
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/upstream>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Dependencies
target_link_libraries(cbsdk_v2
    PUBLIC
        cbproto
        cbshmem
        cbdev
)

# C++17 for implementation, C compatible API
target_compile_features(cbsdk_v2 PUBLIC cxx_std_17)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(cbsdk_v2 PRIVATE wsock32 ws2_32)
elseif(APPLE)
    target_link_libraries(cbsdk_v2 PRIVATE pthread)
else()
    target_link_libraries(cbsdk_v2 PRIVATE pthread)
endif()

# When compiled as SHARED:
# target_compile_definitions(cbsdk_v2 PRIVATE CBSDK_V2_EXPORTS)

# Installation
install(TARGETS cbsdk_v2
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
