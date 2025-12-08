# cbdev - Device Transport Layer
# Handles UDP socket communication with devices

project(cbdev
    DESCRIPTION "CereLink Device Transport Layer"
    LANGUAGES CXX
)

# Library sources
set(CBDEV_SOURCES
    src/device_session.cpp
    src/device_session_311.cpp
    src/device_session_400.cpp
    src/device_session_410.cpp
    src/device_factory.cpp
    src/protocol_detector.cpp
    src/packet_translator.cpp
)

# Build as STATIC library
add_library(cbdev STATIC ${CBDEV_SOURCES})

target_include_directories(cbdev
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Dependencies
target_link_libraries(cbdev
    PUBLIC
        cbproto  # Protocol definitions
)

# C++17 required
target_compile_features(cbdev PUBLIC cxx_std_17)

# Platform-specific networking libraries
if(WIN32)
    target_link_libraries(cbdev PRIVATE wsock32 ws2_32 iphlpapi)

    # Ensure Windows SDK architecture macros are defined.
    # This is required when consumed as a dependency (FetchContent) where the
    # consuming project may not properly propagate architecture settings.
    # Without this, winnt.h fails with "No Target Architecture" error.
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        target_compile_definitions(cbdev PRIVATE _ARM64_)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_compile_definitions(cbdev PRIVATE _AMD64_)
    else()
        target_compile_definitions(cbdev PRIVATE _X86_)
    endif()
elseif(APPLE)
    # macOS networking (includes the IP_BOUND_IF fix!)
    target_link_libraries(cbdev PRIVATE pthread)
else()
    # Linux networking
    target_link_libraries(cbdev PRIVATE pthread)
endif()

# Installation
install(TARGETS cbdev
    EXPORT CBSDKTargets
)

# Install public headers only (minimal API surface)
install(FILES
    include/cbdev/result.h
    include/cbdev/connection.h
    include/cbdev/device_session.h
    include/cbdev/device_factory.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cbdev
)
