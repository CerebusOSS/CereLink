# cbsdk_v2 - SDK Public API
# Orchestrates cbdev + cbshmem to provide clean public C API

project(cbsdk_v2
    DESCRIPTION "CereLink SDK v2"
    LANGUAGES CXX C
)

# Library sources (TODO: add when files are created)
set(CBSDK_V2_SOURCES
    # sdk_session.cpp
    # sdk_api.cpp
    # mode_detection.cpp
    # callback_manager.cpp
)

# For now, create an INTERFACE library placeholder
add_library(cbsdk_v2 INTERFACE)

# TODO: When sources exist, change to:
# add_library(cbsdk_v2 SHARED ${CBSDK_V2_SOURCES})

target_include_directories(cbsdk_v2
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Dependencies
target_link_libraries(cbsdk_v2
    INTERFACE
        cbproto
        cbshmem
        cbdev
)

# C++17 for implementation, C compatible API
target_compile_features(cbsdk_v2 INTERFACE cxx_std_17)

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
