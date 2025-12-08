# cbshm - Shared Memory Management Module
# Handles correct indexing between standalone and client modes

project(cbshm
    DESCRIPTION "CereLink Shared Memory Layer (New Architecture)"
    LANGUAGES CXX
)

# Library sources
set(CBSHMEM_SOURCES
    src/shmem_session.cpp
)

# Build as STATIC library
add_library(cbshm STATIC ${CBSHMEM_SOURCES})

target_include_directories(cbshm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Dependencies
target_link_libraries(cbshm
    PUBLIC
        cbproto  # Protocol definitions
)

# C++17 required
target_compile_features(cbshm PUBLIC cxx_std_17)

# Platform-specific libraries
if(WIN32)
    # Windows shared memory APIs (kernel32 is linked by default)
    target_compile_definitions(cbshm PRIVATE _WIN32_WINNT=0x0601)

elseif(APPLE)
    # macOS shared memory APIs
    target_link_libraries(cbshm PRIVATE pthread)
else()
    # Linux shared memory APIs
    target_link_libraries(cbshm PRIVATE rt pthread)
endif()

# Installation
install(TARGETS cbshm
    EXPORT CBSDKTargets
)
