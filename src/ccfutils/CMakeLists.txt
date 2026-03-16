# CCFUtils - Cerebus Configuration File Utilities
# Standalone library for reading/writing Cerebus configuration files

cmake_minimum_required(VERSION 3.16)

project(ccfutils
    DESCRIPTION "Cerebus Configuration File (CCF) Utilities"
    LANGUAGES CXX
)

##########################################################################################
# Version from parent project
if(NOT DEFINED CCFUTILS_VERSION_MAJOR)
    if(DEFINED PROJECT_VERSION AND NOT "${PROJECT_VERSION}" STREQUAL "")
        string(REPLACE "." ";" VERSION_LIST ${PROJECT_VERSION})
        list(GET VERSION_LIST 0 CCFUTILS_VERSION_MAJOR)
        list(GET VERSION_LIST 1 CCFUTILS_VERSION_MINOR)
        list(LENGTH VERSION_LIST VERSION_LIST_LENGTH)
        if(VERSION_LIST_LENGTH GREATER 2)
            list(GET VERSION_LIST 2 CCFUTILS_VERSION_PATCH)
        else()
            set(CCFUTILS_VERSION_PATCH 0)
        endif()
    else()
        # Default version if not provided by parent
        set(CCFUTILS_VERSION_MAJOR 8)
        set(CCFUTILS_VERSION_MINOR 1)
        set(CCFUTILS_VERSION_PATCH 0)
    endif()
endif()

##########################################################################################
# Dependencies

# PugiXML (for XML parsing)
if(NOT TARGET pugixml::pugixml)
    include(FetchContent)
    set(PUGIXML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(PUGIXML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(PUGIXML_BUILD_SHARED_AND_STATIC OFF)
    set(PUGIXML_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG v1.15
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(pugixml)
    if(TARGET pugixml AND NOT TARGET pugixml::pugixml)
        add_library(pugixml::pugixml ALIAS pugixml)
    endif()
endif()

# cbproto (new modular architecture)
# Expect cbproto to be available either as subdirectory or installed
if(NOT TARGET cbproto)
    # Try to find cbproto as a subdirectory
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../cbproto/CMakeLists.txt)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../cbproto
                         ${CMAKE_CURRENT_BINARY_DIR}/cbproto)
    else()
        # Try to find installed cbproto
        find_package(cbproto REQUIRED)
    endif()
endif()

##########################################################################################
# Library Sources

set(CCFUTILS_SOURCES
    src/CCFUtils.cpp
    src/CCFUtilsBinary.cpp
    src/CCFUtilsConcurrent.cpp
    src/CCFUtilsXml.cpp
    src/CCFUtilsXmlItems.cpp
    src/XmlFile.cpp
    src/ccf_config.cpp
)

set(CCFUTILS_HEADERS
    include/CCFUtils.h
    include/ccfutils/ccf_config.h
)

set(CCFUTILS_INTERNAL_HEADERS
    src/CCFUtilsBinary.h
    src/CCFUtilsConcurrent.h
    src/CCFUtilsXml.h
    src/CCFUtilsXmlItems.h
    src/CCFUtilsXmlItemsGenerate.h
    src/CCFUtilsXmlItemsParse.h
    src/XmlFile.h
    src/XmlItem.h
)

set(CCFUTILS_COMPAT_HEADERS
    include/ccfutils/compat/platform.h
    include/ccfutils/compat/debug.h
)

##########################################################################################
# Build as STATIC library

add_library(ccfutils STATIC
    ${CCFUTILS_SOURCES}
    ${CCFUTILS_HEADERS}
    ${CCFUTILS_INTERNAL_HEADERS}
    ${CCFUTILS_COMPAT_HEADERS}
)

# Version definitions for compatibility headers
target_compile_definitions(ccfutils
    PRIVATE
        CCFUTILS_VERSION_MAJOR=${CCFUTILS_VERSION_MAJOR}
        CCFUTILS_VERSION_MINOR=${CCFUTILS_VERSION_MINOR}
        CCFUTILS_VERSION_PATCH=${CCFUTILS_VERSION_PATCH}
)

# Include directories
target_include_directories(ccfutils
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(ccfutils
    PUBLIC
        cbproto
    PRIVATE
        pugixml::pugixml
)

# Require C++17
target_compile_features(ccfutils PUBLIC cxx_std_17)

# Position-independent code (for linking into shared libraries)
set_target_properties(ccfutils PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

##########################################################################################
# Installation

include(GNUInstallDirs)

# Install library
# Note: We don't export ccfutils targets for now since it's meant to be
# used as part of the larger CBSDK build. If standalone installation is needed,
# the dependencies would need to be handled differently.
# install(TARGETS ccfutils
#     EXPORT ccfutilsTargets
#     LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#     ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
# )

# Install public headers
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
        PATTERN "*.h"
)

# Install export targets
# Commented out for now - see note above about standalone installation
# install(EXPORT ccfutilsTargets
#     FILE ccfutilsTargets.cmake
#     NAMESPACE ccfutils::
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ccfutils
# )

# Create and install package config files
# Commented out for now - see note above about standalone installation
# include(CMakePackageConfigHelpers)
#
# configure_package_config_file(
#     ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ccfutilsConfig.cmake.in
#     ${CMAKE_CURRENT_BINARY_DIR}/ccfutilsConfig.cmake
#     INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ccfutils
# )
#
# write_basic_package_version_file(
#     ${CMAKE_CURRENT_BINARY_DIR}/ccfutilsConfigVersion.cmake
#     VERSION ${PROJECT_VERSION}
#     COMPATIBILITY SameMajorVersion
# )
#
# install(FILES
#     ${CMAKE_CURRENT_BINARY_DIR}/ccfutilsConfig.cmake
#     ${CMAKE_CURRENT_BINARY_DIR}/ccfutilsConfigVersion.cmake
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ccfutils
# )

##########################################################################################
# Optional: Tests
# Uncomment if you want to build tests for ccfutils
# if(CBSDK_BUILD_TEST)
#     add_subdirectory(tests)
# endif()
