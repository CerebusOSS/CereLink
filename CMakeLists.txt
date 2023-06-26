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
# QT:
#     On Mac OS, assuming qt installed with homebrew,
#     run CMake with -DCMAKE_PREFIX_PATH=$(brew --prefix qt)
#     On Windows, run CMake with -DQt6_DIR=path\to\qt\version\platform\lib\cmake\Qt6
#     For example: cmake .. -G "Visual Studio 15 2017 Win64" -DQt6_DIR=C:\Qt\6.2.1\msvc2017_64\lib\cmake\Qt6 -DCMAKE_INSTALL_PREFIX=install -DBUILD_CLI=ON
#


CMAKE_MINIMUM_REQUIRED( VERSION 3.16 )
SET(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake" ${CMAKE_MODULE_PATH})
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
option(BUILD_STATIC "Build static cbsdk library" ON)
option(BUILD_CBMEX "Build Matlab wrapper" ON)
option(BUILD_CBOCT "Build Octave wrapper" ON)
option(BUILD_TEST "Build tests" ON)
option(BUILD_HDF5 "Build HDF5" ON)
option(CBPROTO_311 "Build for protocol 3.11" OFF)

##########################################################################################
# Define target names
SET( LIB_NAME cbsdk )
SET( INSTALL_TARGET_LIST ${LIB_NAME} )
SET( LIB_NAME_STATIC cbsdk_static )
SET( LIB_NAME_CBMEX cbmex )
SET( LIB_NAME_CBOCT cboct )
SET( TEST_ANAOUT_NAME test_analog_out )
SET( N2H5_NAME n2h5 )

##########################################################################################
# Store some platform-specific strings used to construct lib names / folder structures.
IF ((CMAKE_SIZEOF_VOID_P EQUAL 8) AND (NOT APPLE))
    SET( LIB_SUFFIX 64 )
ENDIF()

##########################################################################################
# Misc Configuration
# -Make sure debug builds are recognized
SET(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "Add a postfix, usually d on windows")

# -?
if(${CBPROTO_311})
    add_compile_definitions(CBPROTO_311)
endif()
ADD_DEFINITIONS( -DCBSDK_EXPORTS )
IF( WIN32 )
    # From cbhwlib/cbmex.vcproj: PreprocessorDefinitions="WIN32;_WINDOWS;NO_AFX;WINVER=0x0501;CBSDK_EXPORTS;QT_APP"
    ADD_DEFINITIONS(
        -DWIN32 -D_WINDOWS -DNO_AFX -DNOMINMAX
        -D_CRT_SECURE_NO_WARNINGS
        -D_WINSOCK_DEPRECATED_NO_WARNINGS  # Not necessary if -DUNICODE
    )
ENDIF( WIN32 )
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
cmake_policy(SET CMP0063 NEW) # ENABLE CMP0063: Honor visibility properties for all target types.
cmake_policy(SET CMP0042 NEW) # ENABLE CMP0042: MACOSX_RPATH is enabled by default.

##########################################################################################
# Third party libraries
include(FetchContent)

# -Qt
FIND_PACKAGE(Qt6 REQUIRED COMPONENTS Core)
set(CMAKE_AUTOMOC ON)
SET(QT_LIBRARIES Qt6::Core)
ADD_DEFINITIONS( ${QT_DEFINITIONS} -DQT_APP)

# -PugiXML
find_package(pugixml QUIET)  # NO_SYSTEM_ENVIRONMENT_PATH)
if(NOT pugixml_FOUND)
    message(STATUS "Did not find pugixml, will use FetchContent instead.")
    FetchContent_Declare(
            pugixml
            GIT_REPOSITORY https://github.com/zeux/pugixml.git
            GIT_TAG v1.13
    )
    FetchContent_GetProperties(pugixml)
    if(NOT pugixml_POPULATED)
        FetchContent_Populate(pugixml)
        add_subdirectory(${pugixml_SOURCE_DIR} ${pugixml_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
endif()

# -HDF5
IF(${BUILD_HDF5})
    FIND_PACKAGE( HDF5 COMPONENTS C HL)
ENDIF()

# -Matlab
IF(${BUILD_CBMEX})
    # Try MATLAB locally first, then on MATLAB install
    FIND_PATH( Matlab_INCLUDE_DIRS
        "mex.h"
        "${PROJECT_SOURCE_DIR}/Matlab/include"
    )
    IF ( Matlab_INCLUDE_DIRS )
        # Local Matlab mex libraries are stored in platform-specific paths
        IF ( WIN32 )
            SET( PLATFORM_NAME "win" )
        ELSEIF ( APPLE )
            SET( PLATFORM_NAME "osx" )
        ELSE ( WIN32 )
            SET( PLATFORM_NAME "linux" )
        ENDIF ( WIN32 )
        IF( CMAKE_SIZEOF_VOID_P EQUAL 4 )
            SET( PLATFORM_NAME ${PLATFORM_NAME}32 )
        ELSE( CMAKE_SIZEOF_VOID_P EQUAL 4 )
            SET( PLATFORM_NAME ${PLATFORM_NAME}64 )
        ENDIF( CMAKE_SIZEOF_VOID_P EQUAL 4 )
        IF(MSVC)
            SET( PLATFORM_NAME ${PLATFORM_NAME}/microsoft )
        ELSEIF(WIN32)
            SET( PLATFORM_NAME ${PLATFORM_NAME}/mingw64 )
        ENDIF(MSVC)

        SET( MATLAB_ROOT "${PROJECT_SOURCE_DIR}/Matlab" )
        MESSAGE ( STATUS "Search mex libraries at " ${Matlab_INCLUDE_DIRS}/../lib/${PLATFORM_NAME} )
        FILE( GLOB_RECURSE Matlab_LIBRARIES ${Matlab_INCLUDE_DIRS}/../lib/${PLATFORM_NAME}/libm*.* )
        IF( Matlab_LIBRARIES )
            SET( MATLAB_FOUND 1 )
        ENDIF( Matlab_LIBRARIES )
    ELSE ( Matlab_INCLUDE_DIRS )
        #SET( MATLAB_FIND_DEBUG 1 )
        FIND_PACKAGE( Matlab COMPONENTS MX_LIBRARY)
    ENDIF ( Matlab_INCLUDE_DIRS )
ENDIF()

# -Octave
IF(${BUILD_CBOCT})
    FIND_PACKAGE( Octave )
ENDIF()

# Custom CCFUtils Lib
set( CCF_SOURCE
    ${PROJECT_SOURCE_DIR}/CCFUtils/CCFUtils.cpp
    ${PROJECT_SOURCE_DIR}/CCFUtils/CCFUtils.h
    ${PROJECT_SOURCE_DIR}/CCFUtils/CCFUtilsBinary.cpp
    ${PROJECT_SOURCE_DIR}/CCFUtils/CCFUtilsBinary.h
    ${PROJECT_SOURCE_DIR}/CCFUtils/CCFUtilsConcurrent.cpp
    ${PROJECT_SOURCE_DIR}/CCFUtils/CCFUtilsConcurrent.h
    ${PROJECT_SOURCE_DIR}/CCFUtils/CCFUtilsXml.cpp
    ${PROJECT_SOURCE_DIR}/CCFUtils/CCFUtilsXml.h
    ${PROJECT_SOURCE_DIR}/CCFUtils/CCFUtilsXmlItems.cpp
    ${PROJECT_SOURCE_DIR}/CCFUtils/CCFUtilsXmlItems.h
    ${PROJECT_SOURCE_DIR}/CCFUtils/CCFUtilsXmlItemsGenerate.h
    ${PROJECT_SOURCE_DIR}/CCFUtils/CCFUtilsXmlItemsParse.h
    ${PROJECT_SOURCE_DIR}/CCFUtils/XmlFile.cpp
    ${PROJECT_SOURCE_DIR}/CCFUtils/XmlFile.h
    ${PROJECT_SOURCE_DIR}/CCFUtils/XmlItem.h
)
add_library( CCFUtils OBJECT ${CCF_SOURCE} )
target_link_libraries( CCFUtils PRIVATE pugixml::static)
target_include_directories(CCFUtils
        PRIVATE
            ${PROJECT_SOURCE_DIR}/cbproto
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/CCFUtils>
            $<INSTALL_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/CCFUtils>
        INTERFACE # for unit tests
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/CCFUtils>
)
set_property(TARGET CCFUtils PROPERTY POSITION_INDEPENDENT_CODE ON)

##########################################################################################
# Files/folders common to multiple targets

SET( LIB_INCL_DIRS
    ${PROJECT_SOURCE_DIR}/cbproto
    ${PROJECT_SOURCE_DIR}/cbsdk
    ${PROJECT_SOURCE_DIR}/cbhwlib
    ${PROJECT_SOURCE_DIR}/Central
    ${PROJECT_SOURCE_DIR}/CentralCommon
)
IF( WIN32 )
    LIST(APPEND LIB_INCL_DIRS "${PROJECT_SOURCE_DIR}/compat/")
ENDIF( WIN32 )

SET( LIB_SOURCE
    ${PROJECT_SOURCE_DIR}/cbsdk/cbsdk.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/cbhwlib.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/cbHwlibHi.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/InstNetwork.cpp
    ${PROJECT_SOURCE_DIR}/Central/Instrument.cpp
    ${PROJECT_SOURCE_DIR}/Central/UDPsocket.cpp
)
IF( WIN32 )
    IF(MSVC)
        LIST (APPEND LIB_SOURCE ${PROJECT_SOURCE_DIR}/cbmex/cbMex.rc)
    ENDIF(MSVC)
ENDIF( WIN32 )

# Source for both cbmex and octave targets
SET( LIB_SOURCE_CBMEX
    ${PROJECT_SOURCE_DIR}/cbmex/cbmex.cpp
)
IF( MSVC )
    LIST ( APPEND LIB_SOURCE_CBMEX ${PROJECT_SOURCE_DIR}/cbmex/cbMex.rc )
ENDIF( MSVC )


##########################################################################################
# Targets


##
# cbsdk shared / dynamic
ADD_LIBRARY( ${LIB_NAME} SHARED ${LIB_SOURCE} )
target_include_directories( ${LIB_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cbsdk>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cbhwlib>
        $<INSTALL_INTERFACE:include/cbsdk>
        PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Central>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/CentralCommon>
)
TARGET_LINK_LIBRARIES( ${LIB_NAME}
        PRIVATE ${QT_LIBRARIES}
        PUBLIC CCFUtils)
IF( WIN32 )
    TARGET_LINK_LIBRARIES( ${LIB_NAME} PRIVATE wsock32 ws2_32 winmm )
    target_include_directories(${LIB_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/compat>)
ELSE()
    IF(NOT APPLE)
        TARGET_LINK_LIBRARIES(${LIB_NAME} PRIVATE rt)
        # Hide unexported symbols
        SET_TARGET_PROPERTIES( ${LIB_NAME} PROPERTIES LINK_FLAGS "-Wl,--exclude-libs,ALL" )
    ENDIF(NOT APPLE)
ENDIF( WIN32 )
SET_TARGET_PROPERTIES(
        ${LIB_NAME}
        PROPERTIES
        SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)


##
# cbsdk_static (optional)
IF(${BUILD_STATIC})
    ADD_LIBRARY( ${LIB_NAME_STATIC} STATIC ${LIB_SOURCE} ${LIB_HEADERS_MOC} )
    target_include_directories( ${LIB_NAME_STATIC}
            PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cbsdk>
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cbhwlib>
                $<INSTALL_INTERFACE:include/cbsdk>
            PRIVATE
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Central>
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/CentralCommon>
    )
    TARGET_LINK_LIBRARIES( ${LIB_NAME_STATIC}
            PRIVATE ${QT_LIBRARIES}
            PUBLIC CCFUtils)
    IF( WIN32 )
        target_include_directories(${LIB_NAME_STATIC} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/compat>)
        TARGET_LINK_LIBRARIES( ${LIB_NAME_STATIC} PRIVATE ws2_32 winmm )
        SET_TARGET_PROPERTIES( ${LIB_NAME_STATIC} PROPERTIES
            COMPILE_FLAGS "-DSTATIC_CBSDK_LINK"
            # MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
    ELSE( WIN32 )
        IF(NOT APPLE)
            TARGET_LINK_LIBRARIES(${LIB_NAME_STATIC} PRIVATE rt)
        ENDIF(NOT APPLE)
        # Need relocatable static library
        SET_TARGET_PROPERTIES( ${LIB_NAME_STATIC} PROPERTIES
            LINK_FLAGS "-Wl,--exclude-libs,ALL"
            POSITION_INDEPENDENT_CODE ON)
        #SET_TARGET_PROPERTIES(${LIB_NAME_STATIC} PROPERTIES XCODE_ATTRIBUTE_GENERATE_MASTER_OBJECT_FILE "YES")
        #SET_TARGET_PROPERTIES(${LIB_NAME_STATIC} PROPERTIES XCODE_ATTRIBUTE_STRIP_STYLE "non-global")
        #SET_TARGET_PROPERTIES(${LIB_NAME_STATIC} PROPERTIES XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING "YES")
        #SET_TARGET_PROPERTIES(${LIB_NAME_STATIC} PROPERTIES XCODE_ATTRIBUTE_SEPARATE_STRIP "YES")
    ENDIF( WIN32)
    LIST(APPEND INSTALL_TARGET_LIST ${LIB_NAME_STATIC})
ENDIF(${BUILD_STATIC})

##
# cbmex
IF(${BUILD_CBMEX} AND MATLAB_FOUND )
    MESSAGE ( STATUS "Add cbmex build target using MATLAB libs at " ${Matlab_ROOT_DIR})
    ADD_LIBRARY( ${LIB_NAME_CBMEX} SHARED ${LIB_SOURCE_CBMEX} )
    
    # Want package name to be cbmex without prefix
    IF( WIN32 )
        # Do not output to Debug/Release directories on Windows.
        # Commented out because it causes 'multiple rules generate cbmex.mexw64' error
        # SET_TARGET_PROPERTIES( ${LIB_NAME_CBMEX} PROPERTIES PREFIX "../")
        IF (MSVC)
            # Manually export mexFunction because __declspec(dllexport) conflicts with its definition in mex.h
            SET_TARGET_PROPERTIES( ${LIB_NAME_CBMEX} PROPERTIES
                LINK_FLAGS "/EXPORT:mexFunction"
                # MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
            )
        ENDIF(MSVC)
    ELSEIF( APPLE )
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBMEX} PROPERTIES PREFIX "" )
        # This is for normal users of MATLAB on OSX without homebrew 
        #  so we try to use relative paths to be able to bundle shared libraries 
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBMEX} PROPERTIES BUILD_WITH_INSTALL_RPATH 1 INSTALL_NAME_DIR "@rpath")
    ELSE( WIN32 )
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBMEX} PROPERTIES PREFIX "" )
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBMEX} PROPERTIES LINK_FLAGS "-Wl,--exclude-libs,ALL" )
    ENDIF( WIN32 )
    
    SET_TARGET_PROPERTIES( ${LIB_NAME_CBMEX} PROPERTIES SUFFIX .${Matlab_MEX_EXTENSION})
    IF( NOT CBMEX_INSTALL_PREFIX )
        SET( CBMEX_INSTALL_PREFIX .)
    ENDIF( NOT CBMEX_INSTALL_PREFIX )
    # Use static library to build cbmex
    ADD_DEPENDENCIES( ${LIB_NAME_CBMEX} ${LIB_NAME_STATIC} )
    TARGET_INCLUDE_DIRECTORIES( ${LIB_NAME_CBMEX}
        PRIVATE ${Matlab_INCLUDE_DIRS}
    )
    TARGET_LINK_LIBRARIES( ${LIB_NAME_CBMEX}
        ${LIB_NAME_STATIC}
        ${QT_LIBRARIES}
        ${Matlab_LIBRARIES}
    )
    INSTALL( TARGETS ${LIB_NAME_CBMEX}
        RUNTIME DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink 
        LIBRARY DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink 
        ARCHIVE DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink 
    )
ENDIF( ${BUILD_CBMEX} AND MATLAB_FOUND )

##
# octave
IF( ${BUILD_CBOCT} AND OCTAVE_FOUND )
    MESSAGE ( STATUS "Add cbmex build target using Octave libs at " ${OCTAVE_OCT_LIB_DIR})
    ADD_LIBRARY( ${LIB_NAME_CBOCT} SHARED ${LIB_SOURCE_CBMEX} )
    
    # Want package name to be cbmex without prefix
    IF( WIN32 )
        # Do not output to Debug/Release directories on Windows
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBOCT} PROPERTIES PREFIX "../" )
        # Manually export mexFunction because __declspec(dllexport) conflicts with its definition in mex.h
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBOCT} PROPERTIES LINK_FLAGS "/EXPORT:mexFunction" )
    ELSEIF( APPLE )
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBOCT} PROPERTIES PREFIX "" )
        # This is for normal users of MATLAB on OSX without homebrew 
        #  so we try to use relative paths to be able to bundle shared libraries 
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBOCT} PROPERTIES BUILD_WITH_INSTALL_RPATH 1 INSTALL_NAME_DIR "@rpath")
    ELSE( WIN32 )
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBOCT} PROPERTIES PREFIX "" )
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBOCT} PROPERTIES LINK_FLAGS "-Wl,--exclude-libs,ALL" )
    ENDIF( WIN32 )
    
    SET_TARGET_PROPERTIES( ${LIB_NAME_CBOCT} PROPERTIES SUFFIX .mex)
    IF( NOT CBMEX_INSTALL_PREFIX )
        SET( CBMEX_INSTALL_PREFIX .)
    ENDIF( NOT CBMEX_INSTALL_PREFIX )
    # Use static library to build cbmex
    ADD_DEPENDENCIES( ${LIB_NAME_CBOCT} ${LIB_NAME_STATIC} )
    TARGET_INCLUDE_DIRECTORIES( ${LIB_NAME_CBOCT}
        PRIVATE ${OCTAVE_INCLUDE_DIR}
    )
    TARGET_LINK_LIBRARIES( ${LIB_NAME_CBOCT} ${LIB_NAME_STATIC} ${QT_LIBRARIES} ${OCTAVE_LIBRARIES} )
    INSTALL( TARGETS ${LIB_NAME_CBOCT}
        RUNTIME DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink 
        LIBRARY DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink 
        ARCHIVE DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink 
    )
ENDIF( ${BUILD_CBOCT} AND OCTAVE_FOUND )

# Some Test applications.
# These all use deployqt so let's create some helper functions for that.
function(findQtInstallationTool qtdeploytoolname)
    if(QT_DEPLOYQT_EXECUTABLE)
        return()
    endif()
    get_target_property(QT_QMAKE_EXE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXE}" DIRECTORY)
    find_program (QT_DEPLOYQT_EXECUTABLE ${qtdeploytoolname} HINTS "${QT_BIN_DIR}")
    if (QT_DEPLOYQT_EXECUTABLE)
        message(STATUS "Qt deploy tool found at ${QT_DEPLOYQT_EXECUTABLE}")
    else()
        message(WARNING "Qt deploy tool wasn't found, installing ${PROJECT_NAME} will fail!")
        return()
    endif()
endfunction()

##
# test
IF(${BUILD_TEST})
    enable_testing()
    add_subdirectory(tests)
ENDIF()
# These are not real tests. They are simply utilities to help debug CBSDK.
IF(${BUILD_TEST})
    SET(TEST_TARGET_LIST)

    # testcbsdk is a bit weird because it does not link the library, but adds the library sources.
    #  This is convenient for development, but also annoying because we have to manually add transitive dependencies.
    #  TODO: It would be better if we made a simple obj target then reused that.
    ADD_EXECUTABLE(testcbsdk
            ${PROJECT_SOURCE_DIR}/samples/TestCBSDK/testcbsdk.cpp
            ${LIB_SOURCE} ${LIB_HEADERS_MOC}
    )
    TARGET_INCLUDE_DIRECTORIES(testcbsdk PRIVATE
        ${LIB_INCL_DIRS}
        ${PROJECT_SOURCE_DIR}/cbsdk
    )
    TARGET_LINK_LIBRARIES(testcbsdk PRIVATE ${QT_LIBRARIES} CCFUtils)
    IF(WIN32)
        TARGET_LINK_LIBRARIES(testcbsdk
            PRIVATE
               wsock32 ws2_32 winmm
        )
        target_include_directories(testcbsdk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/compat)
    ELSEIF(NOT APPLE)
        # Hide unexported symbols
        TARGET_LINK_LIBRARIES(testcbsdk PRIVATE rt)
        SET_TARGET_PROPERTIES(testcbsdk PROPERTIES LINK_FLAGS "-Wl,--exclude-libs,ALL" )
    ENDIF(WIN32)
    LIST(APPEND TEST_TARGET_LIST testcbsdk)

    ADD_EXECUTABLE(testccf
            ${PROJECT_SOURCE_DIR}/samples/TestCCF/test_ccf.cpp
    )
    TARGET_LINK_LIBRARIES(testccf ${LIB_NAME} )
    LIST(APPEND TEST_TARGET_LIST testccf)

    ADD_EXECUTABLE(test_io ${PROJECT_SOURCE_DIR}/samples/TestIO/test_io.cpp )
    TARGET_LINK_LIBRARIES(test_io ${LIB_NAME} )
    LIST(APPEND TEST_TARGET_LIST test_io)

    ADD_EXECUTABLE(test_callback ${PROJECT_SOURCE_DIR}/samples/TestIO/test_callback.cpp)
    TARGET_LINK_LIBRARIES(test_callback ${LIB_NAME} )
    LIST(APPEND TEST_TARGET_LIST test_callback)

    ADD_EXECUTABLE(test_comments ${PROJECT_SOURCE_DIR}/samples/TestComments/test_comments.cpp )
    TARGET_LINK_LIBRARIES(test_comments ${LIB_NAME} )
    LIST(APPEND TEST_TARGET_LIST test_comments)

    ADD_EXECUTABLE(test_analog_out ${PROJECT_SOURCE_DIR}/samples/TestAnalogOut/test_analog_out.cpp )
    TARGET_LINK_LIBRARIES(test_analog_out ${LIB_NAME})
    LIST(APPEND TEST_TARGET_LIST test_analog_out)

    LIST(APPEND INSTALL_TARGET_LIST ${TEST_TARGET_LIST})

    IF(WIN32)
        findQtInstallationTool("windeployqt")
        add_custom_command(TARGET testcbsdk POST_BUILD
            COMMAND ${QT_DEPLOYQT_EXECUTABLE} $<TARGET_FILE:${LIB_NAME}>)
        add_custom_command(TARGET test_io POST_BUILD
            COMMAND ${QT_DEPLOYQT_EXECUTABLE} $<TARGET_FILE:${LIB_NAME}>)
        add_custom_command(TARGET test_callback POST_BUILD
                COMMAND ${QT_DEPLOYQT_EXECUTABLE} $<TARGET_FILE:${LIB_NAME}>)
        add_custom_command(TARGET test_comments POST_BUILD
            COMMAND ${QT_DEPLOYQT_EXECUTABLE} $<TARGET_FILE:${LIB_NAME}>)
        add_custom_command(TARGET test_analog_out POST_BUILD
            COMMAND ${QT_DEPLOYQT_EXECUTABLE} $<TARGET_FILE:${LIB_NAME}>)
    ENDIF(WIN32)

    if(NOT CMAKE_INSTALL_RPATH)
        set(LIBDIR "../lib64")
        foreach(test_app ${TEST_TARGET_LIST})
            if(APPLE)
                set_property(TARGET ${test_app} APPEND
                        PROPERTY INSTALL_RPATH "@executable_path/;@executable_path/${LIBDIR};@executable_path/../Frameworks")
            elseif(UNIX)
                set_property(TARGET ${test_app}
                        PROPERTY INSTALL_RPATH "\$ORIGIN:\$ORIGIN/${LIBDIR}")
            endif(APPLE)
        endforeach()
    endif(NOT CMAKE_INSTALL_RPATH)
ENDIF(${BUILD_TEST})

##
# n2h5
IF(${BUILD_HDF5} AND HDF5_FOUND )
    SET( N2H5_SOURCE
        ${PROJECT_SOURCE_DIR}/n2h5/main.cpp
        ${PROJECT_SOURCE_DIR}/n2h5/n2h5.cpp
        ${PROJECT_SOURCE_DIR}/n2h5/n2h5.h
        ${PROJECT_SOURCE_DIR}/n2h5/NevNsx.h
    )
    MESSAGE ( STATUS "Add n2h5 utility build target")
    IF( MSVC )
        SET ( N2H5_SOURCE ${N2H5_SOURCE} ${PROJECT_SOURCE_DIR}/n2h5/res/n2h5_res.rc )
    ENDIF( MSVC )
    ADD_EXECUTABLE( ${N2H5_NAME} ${N2H5_SOURCE} )
    TARGET_INCLUDE_DIRECTORIES( ${N2H5_NAME} PRIVATE ${LIB_INCL_DIRS} ${HDF5_INCLUDE_DIRS})
    TARGET_LINK_LIBRARIES (${N2H5_NAME} ${HDF5_LIBRARIES} ${HDF5_HL_LIBRARIES})
    LIST(APPEND INSTALL_TARGET_LIST ${N2H5_NAME})
ENDIF(${BUILD_HDF5} AND HDF5_FOUND )

##
# C++ / CLI
add_subdirectory(cli)


## SWIG -- not working; segfault on import
#find_package(SWIG REQUIRED)
#include(${SWIG_USE_FILE})
#find_package(Python REQUIRED COMPONENTS Development)
#set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/cbmex/cbmex.i PROPERTIES CPLUSPLUS ON)  # for C++ types like bool
#set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/cbmex/cbmex.i PROPERTIES SWIG_FLAGS "-builtin")
#swig_add_library(cbsdk_swig
#        LANGUAGE python
#        SOURCES ${CMAKE_CURRENT_LIST_DIR}/cbmex/cbmex.i
#)
#set_target_properties(cbsdk_swig PROPERTIES SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)
#set_target_properties(cbsdk_swig PROPERTIES COMPILE_FLAGS "-fvisibility=hidden -fvisibility-inlines-hidden")
#swig_link_libraries(cbsdk_swig PRIVATE cbsdk Python::Python)


#########################################################################################
# Install libraries, test executable, and headers
INSTALL( TARGETS ${INSTALL_TARGET_LIST}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib${LIB_SUFFIX}
    ARCHIVE DESTINATION lib${LIB_SUFFIX}
)
INSTALL(
    FILES
        ${PROJECT_SOURCE_DIR}/cbsdk/cbsdk.h
        ${PROJECT_SOURCE_DIR}/cbproto/cbproto.h
        ${PROJECT_SOURCE_DIR}/cbhwlib/cbhwlib.h
        ${PROJECT_SOURCE_DIR}/CCFUtils/CCFUtils.h
    DESTINATION
        include
)
if(WIN32)
    install(
            FILES
            $<TARGET_RUNTIME_DLLS:${LIB_NAME}>
            TYPE BIN
    )
    install(FILES ${pugixml_BINARY_DIR}/pugixml.lib
            DESTINATION lib${LIB_SUFFIX}
    )
elseif(APPLE)
    install(CODE "
        execute_process(COMMAND codesign --force --deep --sign - ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/lib${LIB_NAME}.dylib)
    ")
endif(WIN32)


# --------- #
# Packaging #
# --------- #
# Child script handles most details.
# Invoke with `cmake --build <cmake_build_dir> --target package`
include(Packing)
