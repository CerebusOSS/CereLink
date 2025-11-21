# Legacy CBSDK Build System
# This builds the old/legacy cbsdk library and related components

cmake_minimum_required(VERSION 3.16)

##########################################################################################
# Optional Targets for Legacy Build
include(CMakeDependentOption)

# These options default to ON when building standalone, OFF when included as a subdirectory
cmake_dependent_option(CBSDK_BUILD_CBMEX "Build Matlab wrapper" OFF
    "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
cmake_dependent_option(CBSDK_BUILD_CBOCT "Build Octave wrapper" OFF
    "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
cmake_dependent_option(CBSDK_BUILD_LEGACY_TEST "Build legacy tests" ON
    "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
cmake_dependent_option(CBSDK_BUILD_LEGACY_SAMPLE "Build legacy sample applications" ON
    "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
cmake_dependent_option(CBSDK_BUILD_TOOLS "Build tools" ON
    "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)

# These options are inherited from parent or available here
option(CBSDK_BUILD_STATIC "Build static cbsdk library" ON)
option(CBPROTO_311 "Build for protocol 3.11" OFF)

##########################################################################################
# Define target names
set(LIB_NAME cbsdk_old)
set(LIB_NAME_STATIC cbsdk_static)
set(LIB_NAME_CBMEX cbmex)
set(LIB_NAME_CBOCT cboct)
set(LEGACY_INSTALL_TARGET_LIST ${LIB_NAME})

##########################################################################################
# Third party libraries

# -PugiXML (required for CCFUtils)
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

##########################################################################################
# CCFUtils Library
# Use the new standalone ccfutils from src/ccfutils
# It should already be built by the parent CMakeLists.txt
if(NOT TARGET ccfutils)
    message(STATUS "Building ccfutils for legacy build")
    add_subdirectory(${PROJECT_SOURCE_DIR}/src/ccfutils
                     ${CMAKE_CURRENT_BINARY_DIR}/ccfutils)
endif()

# Create an alias for backwards compatibility with old code
# Old code expects CCFUtils (uppercase), new target is ccfutils (lowercase)
if(NOT TARGET CCFUtils)
    add_library(CCFUtils ALIAS ccfutils)
endif()

##########################################################################################
# Legacy CBSDK Library Sources
set(LIB_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cbsdk/cbsdk.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cbsdk/ContinuousData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cbsdk/EventData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cbhwlib/cbhwlib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cbhwlib/cbHwlibHi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cbhwlib/InstNetwork.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/central/Instrument.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/central/UDPsocket.cpp
)

##########################################################################################
# cbsdk_old shared / dynamic library
add_library(${LIB_NAME} SHARED ${LIB_SOURCE})
target_include_directories(${LIB_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cbhwlib
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cbproto
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cbsdk
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ccfutils
        ${CMAKE_CURRENT_SOURCE_DIR}/src/central
        ${CMAKE_CURRENT_SOURCE_DIR}/src/compat
)
target_link_libraries(${LIB_NAME}
    PRIVATE
        CCFUtils
        $<BUILD_INTERFACE:pugixml::pugixml>)

if(WIN32)
    target_link_libraries(${LIB_NAME} PRIVATE wsock32 ws2_32 winmm)
    target_include_directories(${LIB_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/compat>)
else()
    if(NOT APPLE)
        target_link_libraries(${LIB_NAME} PRIVATE rt)
        # Hide unexported symbols
        target_link_options(${LIB_NAME} PRIVATE "LINKER:--exclude-libs,ALL")
    endif(NOT APPLE)
endif(WIN32)

set_target_properties(${LIB_NAME}
    PROPERTIES
    SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)
target_compile_definitions(${LIB_NAME} PRIVATE CBSDK_EXPORTS)

##########################################################################################
# cbsdk_static (optional)
if(CBSDK_BUILD_STATIC)
    add_library(${LIB_NAME_STATIC} STATIC ${LIB_SOURCE})
    target_include_directories(${LIB_NAME_STATIC}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/cbhwlib
            ${CMAKE_CURRENT_SOURCE_DIR}/src/cbproto
            ${CMAKE_CURRENT_SOURCE_DIR}/src/cbsdk
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/ccfutils
            ${CMAKE_CURRENT_SOURCE_DIR}/src/central
            ${CMAKE_CURRENT_SOURCE_DIR}/src/compat
    )
    target_link_libraries(${LIB_NAME_STATIC}
        PRIVATE
            CCFUtils
            $<BUILD_INTERFACE:pugixml::pugixml>)

    if(WIN32)
        target_link_libraries(${LIB_NAME_STATIC} PRIVATE ws2_32 winmm)
        target_compile_definitions(${LIB_NAME_STATIC} PUBLIC STATIC_CBSDK_LINK)
    else(WIN32)
        if(NOT APPLE)
            target_link_libraries(${LIB_NAME_STATIC} PRIVATE rt)
        endif(NOT APPLE)
        # Need relocatable static library
        target_link_options(${LIB_NAME_STATIC} PRIVATE "LINKER:--exclude-libs,ALL")
        set_target_properties(${LIB_NAME_STATIC} PROPERTIES
            POSITION_INDEPENDENT_CODE ON)
    endif(WIN32)

    include(CheckAtomicNeeded)
    check_and_link_atomic(${LIB_NAME_STATIC})
    list(APPEND LEGACY_INSTALL_TARGET_LIST ${LIB_NAME_STATIC})
endif(CBSDK_BUILD_STATIC)

##########################################################################################
# Legacy Tests
if(CBSDK_BUILD_LEGACY_TEST)
    # GoogleTest dependency
    if(NOT TARGET gtest)
        message(STATUS "Fetching GoogleTest for legacy tests")
        include(FetchContent)
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.15.2
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(googletest)
    endif()

    enable_testing()

    # CCFUtils Tests
    add_executable(ccfutils_tests
        ${PROJECT_SOURCE_DIR}/tests/CCFUtilsTests.cpp
        ${PROJECT_SOURCE_DIR}/tests/test_main.cpp
    )
    target_include_directories(ccfutils_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cbproto
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cbsdk
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cbhwlib
        ${CMAKE_CURRENT_SOURCE_DIR}/src/central
        ${CMAKE_CURRENT_SOURCE_DIR}/src/compat
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/ccfutils
    )
    target_link_libraries(ccfutils_tests PRIVATE gtest ${LIB_NAME_STATIC} CCFUtils)
    target_compile_definitions(ccfutils_tests PRIVATE
        PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME ccfutils_tests COMMAND ccfutils_tests)

    # Synchronization Tests
    add_executable(sync_tests
        ${PROJECT_SOURCE_DIR}/tests/SynchronizationTests.cpp
        ${PROJECT_SOURCE_DIR}/tests/test_main.cpp
    )
    target_link_libraries(sync_tests PRIVATE gtest)
    add_test(NAME sync_tests COMMAND sync_tests)

    # ContinuousData Tests
    add_executable(continuous_data_tests
        ${PROJECT_SOURCE_DIR}/tests/ContinuousDataTests.cpp
        ${PROJECT_SOURCE_DIR}/tests/test_main.cpp
    )
    target_include_directories(continuous_data_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cbproto
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cbsdk
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cbhwlib
        ${CMAKE_CURRENT_SOURCE_DIR}/src/central
        ${CMAKE_CURRENT_SOURCE_DIR}/src/compat
    )
    target_link_libraries(continuous_data_tests PRIVATE gtest ${LIB_NAME_STATIC})
    add_test(NAME continuous_data_tests COMMAND continuous_data_tests)

    # EventData Tests
    add_executable(event_data_tests
        ${PROJECT_SOURCE_DIR}/tests/EventDataTests.cpp
        ${PROJECT_SOURCE_DIR}/tests/test_main.cpp
    )
    target_include_directories(event_data_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cbproto
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cbsdk
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cbhwlib
        ${CMAKE_CURRENT_SOURCE_DIR}/src/central
        ${CMAKE_CURRENT_SOURCE_DIR}/src/compat
    )
    target_link_libraries(event_data_tests PRIVATE gtest ${LIB_NAME_STATIC})
    add_test(NAME event_data_tests COMMAND event_data_tests)
endif(CBSDK_BUILD_LEGACY_TEST)

##########################################################################################
# Legacy Sample Applications
if(CBSDK_BUILD_LEGACY_SAMPLE)
    set(SIMPLE_EXAMPLES
        "simple_cbsdk:SimpleCBSDK/simple_cbsdk.cpp"
        "simple_ccf:SimpleCCF/simple_ccf.cpp"
        "simple_io:SimpleIO/simple_io.cpp"
        "simple_callback:SimpleIO/simple_callback.cpp"
        "simple_comments:SimpleComments/simple_comments.cpp"
        "simple_analog_out:SimpleAnalogOut/simple_analog_out.cpp"
    )

    set(LEGACY_SAMPLE_TARGET_LIST)

    foreach(example ${SIMPLE_EXAMPLES})
        string(REPLACE ":" ";" example_parts ${example})
        list(GET example_parts 0 target_name)
        list(GET example_parts 1 source_file)

        add_executable(${target_name} ${CMAKE_CURRENT_SOURCE_DIR}/examples/${source_file})
        target_link_libraries(${target_name} ${LIB_NAME_STATIC})
        list(APPEND LEGACY_SAMPLE_TARGET_LIST ${target_name})
    endforeach()

    list(APPEND LEGACY_INSTALL_TARGET_LIST ${LEGACY_SAMPLE_TARGET_LIST})
endif(CBSDK_BUILD_LEGACY_SAMPLE)

##########################################################################################
# Language Bindings (MATLAB, Octave, C++/CLI)

# Find MATLAB if needed
if(${CBSDK_BUILD_CBMEX})
    find_path(Matlab_INCLUDE_DIRS "mex.h"
        "${PROJECT_SOURCE_DIR}/wrappers/Matlab/include")

    if(Matlab_INCLUDE_DIRS)
        # Local Matlab mex libraries are stored in platform-specific paths
        if(WIN32)
            set(PLATFORM_NAME "win")
        elseif(APPLE)
            set(PLATFORM_NAME "osx")
        else(WIN32)
            set(PLATFORM_NAME "linux")
        endif(WIN32)

        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(PLATFORM_NAME ${PLATFORM_NAME}32)
        else(CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(PLATFORM_NAME ${PLATFORM_NAME}64)
        endif(CMAKE_SIZEOF_VOID_P EQUAL 4)

        if(MSVC)
            set(PLATFORM_NAME ${PLATFORM_NAME}/microsoft)
        elseif(WIN32)
            set(PLATFORM_NAME ${PLATFORM_NAME}/mingw64)
        endif(MSVC)

        set(MATLAB_ROOT "${PROJECT_SOURCE_DIR}/wrappers/Matlab")
        message(STATUS "Search mex libraries at ${Matlab_INCLUDE_DIRS}/../lib/${PLATFORM_NAME}")
        file(GLOB_RECURSE Matlab_LIBRARIES ${Matlab_INCLUDE_DIRS}/../lib/${PLATFORM_NAME}/libm*.*)
        if(Matlab_LIBRARIES)
            set(MATLAB_FOUND 1)
        endif(Matlab_LIBRARIES)
    else(Matlab_INCLUDE_DIRS)
        find_package(Matlab COMPONENTS MX_LIBRARY)
    endif(Matlab_INCLUDE_DIRS)
endif()

# Find Octave if needed
if(${CBSDK_BUILD_CBOCT})
    find_package(Octave)
endif()

# Source for both cbmex and octave targets
set(LIB_SOURCE_CBMEX
    ${PROJECT_SOURCE_DIR}/wrappers/cbmex/cbmex.cpp
)
if(MSVC)
    list(APPEND LIB_SOURCE_CBMEX ${PROJECT_SOURCE_DIR}/wrappers/cbmex/cbMex.rc)
endif(MSVC)

# cbmex target
if(${CBSDK_BUILD_CBMEX} AND MATLAB_FOUND)
    message(STATUS "Add cbmex build target using MATLAB libs at ${Matlab_ROOT_DIR}")
    add_library(${LIB_NAME_CBMEX} SHARED ${LIB_SOURCE_CBMEX})

    if(WIN32)
        if(MSVC)
            # Manually export mexFunction
            set_target_properties(${LIB_NAME_CBMEX} PROPERTIES
                LINK_FLAGS "/EXPORT:mexFunction")
        endif(MSVC)
    elseif(APPLE)
        set_target_properties(${LIB_NAME_CBMEX} PROPERTIES PREFIX "")
        set_target_properties(${LIB_NAME_CBMEX} PROPERTIES
            BUILD_WITH_INSTALL_RPATH 1 INSTALL_NAME_DIR "@rpath")
    else(WIN32)
        set_target_properties(${LIB_NAME_CBMEX} PROPERTIES PREFIX "")
        set_target_properties(${LIB_NAME_CBMEX} PROPERTIES
            LINK_FLAGS "-Wl,--exclude-libs,ALL")
    endif(WIN32)

    set_target_properties(${LIB_NAME_CBMEX} PROPERTIES
        SUFFIX .${Matlab_MEX_EXTENSION})

    if(NOT CBMEX_INSTALL_PREFIX)
        set(CBMEX_INSTALL_PREFIX .)
    endif(NOT CBMEX_INSTALL_PREFIX)

    add_dependencies(${LIB_NAME_CBMEX} ${LIB_NAME_STATIC})
    target_include_directories(${LIB_NAME_CBMEX} PRIVATE ${Matlab_INCLUDE_DIRS})
    target_link_libraries(${LIB_NAME_CBMEX} ${LIB_NAME_STATIC} ${Matlab_LIBRARIES})

    install(TARGETS ${LIB_NAME_CBMEX}
        RUNTIME DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink
        LIBRARY DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink
        ARCHIVE DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink
    )
endif(${CBSDK_BUILD_CBMEX} AND MATLAB_FOUND)

# octave target
if(${CBSDK_BUILD_CBOCT} AND OCTAVE_FOUND)
    message(STATUS "Add cbmex build target using Octave libs at ${OCTAVE_OCT_LIB_DIR}")
    add_library(${LIB_NAME_CBOCT} SHARED ${LIB_SOURCE_CBMEX})

    if(WIN32)
        set_target_properties(${LIB_NAME_CBOCT} PROPERTIES PREFIX "../")
        set_target_properties(${LIB_NAME_CBOCT} PROPERTIES
            LINK_FLAGS "/EXPORT:mexFunction")
    elseif(APPLE)
        set_target_properties(${LIB_NAME_CBOCT} PROPERTIES PREFIX "")
        set_target_properties(${LIB_NAME_CBOCT} PROPERTIES
            BUILD_WITH_INSTALL_RPATH 1 INSTALL_NAME_DIR "@rpath")
    else(WIN32)
        set_target_properties(${LIB_NAME_CBOCT} PROPERTIES PREFIX "")
        set_target_properties(${LIB_NAME_CBOCT} PROPERTIES
            LINK_FLAGS "-Wl,--exclude-libs,ALL")
    endif(WIN32)

    set_target_properties(${LIB_NAME_CBOCT} PROPERTIES SUFFIX .mex)

    if(NOT CBMEX_INSTALL_PREFIX)
        set(CBMEX_INSTALL_PREFIX .)
    endif(NOT CBMEX_INSTALL_PREFIX)

    add_dependencies(${LIB_NAME_CBOCT} ${LIB_NAME_STATIC})
    target_include_directories(${LIB_NAME_CBOCT} PRIVATE ${OCTAVE_INCLUDE_DIR})
    target_link_libraries(${LIB_NAME_CBOCT} ${LIB_NAME_STATIC} ${OCTAVE_LIBRARIES})

    install(TARGETS ${LIB_NAME_CBOCT}
        RUNTIME DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink
        LIBRARY DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink
        ARCHIVE DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink
    )
endif(${CBSDK_BUILD_CBOCT} AND OCTAVE_FOUND)

# C++/CLI bindings (if they exist)
if(EXISTS ${PROJECT_SOURCE_DIR}/bindings/cli/CMakeLists.txt)
    add_subdirectory(${PROJECT_SOURCE_DIR}/bindings/cli
        ${CMAKE_CURRENT_BINARY_DIR}/cli)
endif()

##########################################################################################
# Tools (n2h5, loop_tester)
if(CBSDK_BUILD_TOOLS)
    if(EXISTS ${PROJECT_SOURCE_DIR}/tools/n2h5/CMakeLists.txt)
        add_subdirectory(${PROJECT_SOURCE_DIR}/tools/n2h5
            ${CMAKE_CURRENT_BINARY_DIR}/n2h5)
    endif()
    if(EXISTS ${PROJECT_SOURCE_DIR}/tools/loop_tester/CMakeLists.txt)
        add_subdirectory(${PROJECT_SOURCE_DIR}/tools/loop_tester
            ${CMAKE_CURRENT_BINARY_DIR}/loop_tester)
    endif()
endif(CBSDK_BUILD_TOOLS)

##########################################################################################
# Installation
# Note: Export is commented out for now since legacy libraries link to ccfutils
# which has its own dependencies. For installation, the parent project should
# handle the complete export set.
# install(TARGETS ${LEGACY_INSTALL_TARGET_LIST}
#     EXPORT CBSDKLegacyTargets
#     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
#     LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#     ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
# )

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/cerelink
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(WIN32)
    install(FILES $<TARGET_RUNTIME_DLLS:${LIB_NAME}> TYPE BIN)
elseif(APPLE)
    install(CODE "
        execute_process(COMMAND codesign --force --deep --sign -
            ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/lib${LIB_NAME}.dylib)
    ")
endif(WIN32)

# Install export targets (commented out - see note above)
# install(EXPORT CBSDKLegacyTargets
#     FILE CBSDKLegacyTargets.cmake
#     NAMESPACE CBSDK::
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CBSDK
# )
