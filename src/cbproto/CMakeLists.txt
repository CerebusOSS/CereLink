# cbproto - Protocol Definitions Module
# Pure header-only library containing packet structures and protocol constants

project(cbproto
    DESCRIPTION "CereLink Protocol Definitions"
    LANGUAGES CXX
)

# Header-only library
add_library(cbproto INTERFACE)

target_include_directories(cbproto
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# C++17 required
target_compile_features(cbproto INTERFACE cxx_std_17)

# Installation
install(TARGETS cbproto
    EXPORT CBSDKTargets
)

# TODO: Add headers when they exist
# install(
#     DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
#     DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
# )
