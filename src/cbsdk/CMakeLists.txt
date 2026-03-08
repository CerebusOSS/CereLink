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

# When compiled as SHARED:
# target_compile_definitions(cbsdk PRIVATE CBSDK_EXPORTS)

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
