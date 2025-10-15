# CBSDK CMake Build System
# Author: griffin.milsap@gmail.com
#         chadwick.boulay@gmail.com
# Version 1.1 Jan. 2017
#   Build information for
#       cbsdk Library
#       -shared, -static (optional), -matlab (optional), -octave (optional)
#       -testcbsdk Test Binary (optional)
#   See "Optional Targets" below for options to disable specific targets.
#
# MATLAB:
#    First ../Matlab  is looked.
#    If mex.h is not found then FindMatlab.cmake looks at usual places.
#    Use -DMatlab_ROOT_DIR if installed in unknown location (or copy to ../Matlab).
#    e.g., Matlab R2016a on Mac OS: -DMatlab_ROOT_DIR=/Applications/MATLAB_R2016a.app/
#    -DCBMEX_INSTALL_PREFIX can be used to install cbmex to given directory
#
# General Windows:
#    - If you are using MSVC and not logged-in as an administrator then you will not be
#    able to build the INSTALL target, as by default it tries to install to C:\Program Files.
#    Change the install target using CMAKE_INSTALL_PREFIX. See example CMAKE command below.
#    - If you would like to build the C# libraries and examples then add -DBUILD_CLI=ON
# NOTE: Qt dependency has been completely removed (replaced with C++ standard library)
#


cmake_minimum_required( VERSION 3.16 )
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(AppleSilicon)
# cmake_policy(SET CMP0091 NEW)

project(CBSDK
        DESCRIPTION "Blackrock Neurotech CereBus Software Development Kit"
        LANGUAGES C CXX
        VERSION 7.6.2
        )

# Common Configuration
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)

##########################################################################################
# Optional Targets
include(CMakeDependentOption)

# These options default to ON when building standalone, OFF when included as a subdirectory
cmake_dependent_option(CBSDK_BUILD_CBMEX "Build Matlab wrapper" ON
    "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
cmake_dependent_option(CBSDK_BUILD_CBOCT "Build Octave wrapper" ON
    "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
cmake_dependent_option(CBSDK_BUILD_TEST "Build tests" ON
    "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
cmake_dependent_option(CBSDK_BUILD_SAMPLE "Build sample applications" ON
    "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
cmake_dependent_option(CBSDK_BUILD_HDF5 "Build HDF5" ON
    "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)

# These options are always available regardless of build context
option(CBSDK_BUILD_STATIC "Build static cbsdk library" ON)
option(CBPROTO_311 "Build for protocol 3.11" OFF)

##########################################################################################
# Define target names
set( LIB_NAME cbsdk )
set( INSTALL_TARGET_LIST ${LIB_NAME} )
set( LIB_NAME_STATIC cbsdk_static )
set( LIB_NAME_CBMEX cbmex )
set( LIB_NAME_CBOCT cboct )
set( TEST_ANAOUT_NAME test_analog_out )
set( N2H5_NAME n2h5 )

##########################################################################################
# Misc Configuration
# -Make sure debug builds are recognized
set(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "Add a postfix, usually d on windows")
# -Force universal binary on macOS
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")

# -?
if(CBPROTO_311)
    add_compile_definitions(CBPROTO_311)
endif()
if( WIN32 )
    # From cbhwlib/cbmex.vcproj: PreprocessorDefinitions="WIN32;_WINDOWS;NO_AFX;WINVER=0x0501;CBSDK_EXPORTS"
    add_compile_definitions(
        WIN32 _WINDOWS NO_AFX NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS  # Not necessary if -DUNICODE
    )
endif( WIN32 )
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
cmake_policy(SET CMP0063 NEW) # ENABLE CMP0063: Honor visibility properties for all target types.
cmake_policy(SET CMP0042 NEW) # ENABLE CMP0042: MACOSX_RPATH is enabled by default.

##########################################################################################
# Standard installation directories
include(GNUInstallDirs)

##########################################################################################
# Third party libraries
include(FetchContent)

# -PugiXML
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

# Custom CCFUtils Lib
set( CCF_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ccfutils/CCFUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ccfutils/CCFUtilsBinary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ccfutils/CCFUtilsConcurrent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ccfutils/CCFUtilsXml.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ccfutils/CCFUtilsXmlItems.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ccfutils/XmlFile.cpp
)
add_library(CCFUtils OBJECT ${CCF_SOURCE})
target_link_libraries(CCFUtils PRIVATE pugixml::pugixml)
target_include_directories(CCFUtils
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        INTERFACE # for unit tests
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/ccfutils>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/cbproto
)
set_property(TARGET CCFUtils PROPERTY POSITION_INDEPENDENT_CODE ON)

##########################################################################################
# Files/folders common to multiple targets
# (LIB_INCL_DIRS removed - now using target_include_directories)

set( LIB_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cbsdk/cbsdk.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cbhwlib/cbhwlib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cbhwlib/cbHwlibHi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cbhwlib/InstNetwork.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/central/Instrument.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/central/UDPsocket.cpp
)


##########################################################################################
# Targets

# cbsdk shared / dynamic
add_library(${LIB_NAME} SHARED ${LIB_SOURCE} $<TARGET_OBJECTS:CCFUtils>)
target_include_directories( ${LIB_NAME}
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
target_link_libraries( ${LIB_NAME}
        PRIVATE $<BUILD_INTERFACE:pugixml::pugixml>)
if( WIN32 )
    target_link_libraries( ${LIB_NAME} PRIVATE wsock32 ws2_32 winmm )
    target_include_directories(${LIB_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/compat>)
else()
    if(NOT APPLE)
        target_link_libraries(${LIB_NAME} PRIVATE rt)
        # Hide unexported symbols
        target_link_options( ${LIB_NAME} PRIVATE "LINKER:--exclude-libs,ALL" )
    endif(NOT APPLE)
endif( WIN32 )
set_target_properties(
        ${LIB_NAME}
        PROPERTIES
        SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)
target_compile_definitions(${LIB_NAME} PRIVATE CBSDK_EXPORTS)


##
# cbsdk_static (optional)
if(CBSDK_BUILD_STATIC)
    add_library( ${LIB_NAME_STATIC} STATIC ${LIB_SOURCE} $<TARGET_OBJECTS:CCFUtils> )
    target_include_directories( ${LIB_NAME_STATIC}
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
    target_link_libraries( ${LIB_NAME_STATIC}
            PRIVATE $<BUILD_INTERFACE:pugixml::pugixml>)
    if( WIN32 )
        target_link_libraries( ${LIB_NAME_STATIC} PRIVATE ws2_32 winmm )
        target_compile_definitions( ${LIB_NAME_STATIC} PUBLIC STATIC_CBSDK_LINK )
        # Note: If needed, set MSVC runtime library with:
        # set_target_properties( ${LIB_NAME_STATIC} PROPERTIES
        #     MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        # )
    else( WIN32 )
        if(NOT APPLE)
            target_link_libraries(${LIB_NAME_STATIC} PRIVATE rt)
        endif(NOT APPLE)
        # Need relocatable static library
        target_link_options( ${LIB_NAME_STATIC} PRIVATE "LINKER:--exclude-libs,ALL" )
        set_target_properties( ${LIB_NAME_STATIC} PROPERTIES
            POSITION_INDEPENDENT_CODE ON)
    endif( WIN32)
    list(APPEND INSTALL_TARGET_LIST ${LIB_NAME_STATIC})
endif(CBSDK_BUILD_STATIC)

##
# Tests
# Note: We use enable_testing() instead of include(CTest) to avoid dashboard targets clutter
#       The dashboard targets (Experimental*, Nightly*, Continuous*) are only created by include(CTest)
if(CBSDK_BUILD_TEST)
    enable_testing()
    add_subdirectory(tests)
endif(CBSDK_BUILD_TEST)

##
# Very Simple Sample Applications
if(CBSDK_BUILD_SAMPLE)
    add_subdirectory(examples)
endif(CBSDK_BUILD_SAMPLE)

##
# Language bindings (MATLAB, Octave, C++/CLI, Python)
add_subdirectory(bindings)


#########################################################################################
# Install libraries, test executable, and headers
install( TARGETS ${INSTALL_TARGET_LIST}
    EXPORT CBSDKTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(
    DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/include/cerelink
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}
)
if(WIN32)
    install(
            FILES
            $<TARGET_RUNTIME_DLLS:${LIB_NAME}>
            TYPE BIN
    )

elseif(APPLE)
    install(CODE "
        execute_process(COMMAND codesign --force --deep --sign - ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/lib${LIB_NAME}.dylib)
    ")
endif(WIN32)

#########################################################################################
# CMake Package Config
# Install the export set for use with find_package()
include(CMakePackageConfigHelpers)

# Install the export targets
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


# --------- #
# Packaging #
# --------- #
# Child script handles most details.
# Invoke with `cmake --build <cmake_build_dir> --target package`
include(Packing)
