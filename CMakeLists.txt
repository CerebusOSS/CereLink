# CereLink New Modular Architecture Build System
# Author: chadwick.boulay@gmail.com
#
# This builds the new modular cbdev-based architecture.
# For the legacy cbsdk library and tools, see old/CMakeLists.txt

cmake_minimum_required(VERSION 3.16)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(AppleSilicon)
include(GetVersionFromGit)
include(CheckAtomicNeeded)

# Get version from git tags
get_version_from_git()

project(CBSDK
    DESCRIPTION "Blackrock Neurotech CereBus Software Development Kit"
    LANGUAGES C CXX
    VERSION ${GIT_VERSION_FULL}
)

# Common Configuration
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)

##########################################################################################
# Optional Targets
include(CMakeDependentOption)

# Build options for new architecture
cmake_dependent_option(CBSDK_BUILD_TEST "Build tests" ON
    "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
cmake_dependent_option(CBSDK_BUILD_SAMPLE "Build sample applications" ON
    "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)

# Legacy build option
option(CBSDK_BUILD_LEGACY "Build legacy cbsdk library and tools" OFF)

##########################################################################################
# Misc Configuration
# -Make sure debug builds are recognized
set(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "Add a postfix, usually d on windows")

# -Force universal binary on macOS
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")

if(WIN32)
    # Preprocessor definitions for Windows
    add_compile_definitions(
        WIN32 _WINDOWS NO_AFX NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
    )
endif(WIN32)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
cmake_policy(SET CMP0063 NEW) # Honor visibility properties for all target types
cmake_policy(SET CMP0042 NEW) # MACOSX_RPATH is enabled by default

##########################################################################################
# Standard installation directories
include(GNUInstallDirs)

##########################################################################################
# New Modular Architecture
message(STATUS "Building modular architecture")
add_subdirectory(src/cbproto)
add_subdirectory(src/ccfutils)
add_subdirectory(src/cbshm)
add_subdirectory(src/cbdev)
add_subdirectory(src/cbsdk)

##########################################################################################
# Tests for Modular Architecture
if(CBSDK_BUILD_TEST)
    enable_testing()
    add_subdirectory(tests/unit)
    add_subdirectory(tests/integration)
endif(CBSDK_BUILD_TEST)

##########################################################################################
# Sample Applications for New Architecture
if(CBSDK_BUILD_SAMPLE)
    # New architecture examples
    set(CBSDK_EXAMPLES
        "gemini_example:GeminiExample/gemini_example.cpp"
        "simple_device:SimpleDevice/simple_device.cpp"
    )

    set(CBDEV_EXAMPLES
        "check_protocol_version:CheckProtocolVersion/check_protocol_version.cpp"
    )

    set(SAMPLE_TARGET_LIST)

    # cbsdk examples
    foreach(example ${CBSDK_EXAMPLES})
        string(REPLACE ":" ";" example_parts ${example})
        list(GET example_parts 0 target_name)
        list(GET example_parts 1 source_file)

        add_executable(${target_name} ${CMAKE_CURRENT_SOURCE_DIR}/examples/${source_file})
        target_link_libraries(${target_name} cbsdk)
        list(APPEND SAMPLE_TARGET_LIST ${target_name})
    endforeach()

    # cbdev examples (link against cbdev only)
    foreach(example ${CBDEV_EXAMPLES})
        string(REPLACE ":" ";" example_parts ${example})
        list(GET example_parts 0 target_name)
        list(GET example_parts 1 source_file)

        add_executable(${target_name} ${CMAKE_CURRENT_SOURCE_DIR}/examples/${source_file})
        target_link_libraries(${target_name} cbdev)
        list(APPEND SAMPLE_TARGET_LIST ${target_name})
    endforeach()

    # Set RPATH for examples
    if(NOT CMAKE_INSTALL_RPATH)
        set(LIBDIR "../lib")
        foreach(sample_app ${SAMPLE_TARGET_LIST})
            if(APPLE)
                set_property(TARGET ${sample_app} APPEND
                    PROPERTY INSTALL_RPATH "@executable_path/;@executable_path/${LIBDIR};@executable_path/../Frameworks")
            elseif(UNIX)
                set_property(TARGET ${sample_app}
                    PROPERTY INSTALL_RPATH "\$ORIGIN:\$ORIGIN/${LIBDIR}")
            endif(APPLE)
        endforeach()
    endif(NOT CMAKE_INSTALL_RPATH)
endif(CBSDK_BUILD_SAMPLE)

##########################################################################################
# Legacy Build (Optional)
if(CBSDK_BUILD_LEGACY)
    message(STATUS "Building legacy cbsdk library and tools")
    add_subdirectory(old)
endif(CBSDK_BUILD_LEGACY)

##########################################################################################
# Installation for new architecture
# (Legacy installation is handled in old/CMakeLists.txt)

# The actual installation targets are defined in the subdirectories:
# - src/cbproto/CMakeLists.txt installs cbproto
# - src/cbshm/CMakeLists.txt installs cbshm
# - src/cbdev/CMakeLists.txt installs cbdev
# - src/cbsdk/CMakeLists.txt installs cbsdk

# Install sample applications if built
if(CBSDK_BUILD_SAMPLE AND SAMPLE_TARGET_LIST)
    install(TARGETS ${SAMPLE_TARGET_LIST}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

##########################################################################################
# CMake Package Config for new architecture
include(CMakePackageConfigHelpers)

# Install the export targets (collected from subdirectories)
install(EXPORT CBSDKTargets
    FILE CBSDKTargets.cmake
    NAMESPACE CBSDK::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CBSDK
)

# Create a basic config file
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CBSDKConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/CBSDKConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CBSDK
)

# Create a version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/CBSDKConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the config and version files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/CBSDKConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/CBSDKConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CBSDK
)

##########################################################################################
# Packaging
# Invoke with `cmake --build <cmake_build_dir> --target package`
include(Packing)
