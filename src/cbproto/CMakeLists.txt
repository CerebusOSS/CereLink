# cbproto - Protocol Definitions Module
# Protocol packet structures, constants, connection enums, and packet translation

project(cbproto
    DESCRIPTION "CereLink Protocol Definitions"
    LANGUAGES CXX
)

# Library sources
set(CBPROTO_SOURCES
    src/packet_translator.cpp
)

# Build as STATIC library (needed for PacketTranslator implementation)
add_library(cbproto STATIC ${CBPROTO_SOURCES})

target_include_directories(cbproto
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# C++17 required
target_compile_features(cbproto PUBLIC cxx_std_17)

# Installation
install(TARGETS cbproto
    EXPORT CBSDKTargets
)

# TODO: Add headers when they exist
# install(
#     DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
#     DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
# )
