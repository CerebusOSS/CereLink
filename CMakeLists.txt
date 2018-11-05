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
# QT:
#     QT5 ships with its own cmake config files. We need to tell CMake where to find this.
#     On Mac OS, assuming qt installed with homebrew,
#     run CMake with -DCMAKE_PREFIX_PATH=$(brew --prefix qt5)
#     On Windows, run CMake with -DQt5_DIR=path\to\qt\version\platform\lib\cmake\Qt5
#     For example: cmake .. -G "Visual Studio 14 2015 Win64" -DQt5_DIR=C:\Qt\5.10.0\msvc2015_64\lib\cmake\Qt5


CMAKE_MINIMUM_REQUIRED( VERSION 3.1.0 )

PROJECT( CBSDK )

SET( CBSDK_VERSION_MAJOR 1 )
SET( CBSDK_VERSION_MINOR 0 )

##########################################################################################
# Optional Targets
option(BUILD_STATIC "Build static cbsdk library" ON)
option(BUILD_CBMEX "Build Matlab wrapper" ON)
option(BUILD_CBOCT "Build Octave wrapper" ON)
option(BUILD_TEST "Build testcbsdk" ON)
option(BUILD_HDF5 "Build HDF5" ON)

##########################################################################################
# Define target names
SET( LIB_NAME cbsdk )
SET( INSTALL_TARGET_LIST ${LIB_NAME} )
SET( LIB_NAME_STATIC cbsdk_static )
SET( LIB_NAME_CBMEX cbmex )
SET( LIB_NAME_CBOCT cboct )
SET( TEST_NAME testcbsdk )
SET( N2H5_NAME n2h5 )

##########################################################################################
# Store some platform-specific strings used to construct lib names / folder structures.
IF ( WIN32 )
    SET( PLATFORM_NAME "win" )
ELSEIF ( APPLE )
    SET( PLATFORM_NAME "osx" )
ELSE ( WIN32 )
    SET( PLATFORM_NAME "linux" )
ENDIF ( WIN32 )
# Handle 64 bit binaries
IF( CMAKE_SIZEOF_VOID_P EQUAL 4 )
    SET( LIB_SUFFIX "" )
    SET( PLATFORM_NAME ${PLATFORM_NAME}32 )
ELSE( CMAKE_SIZEOF_VOID_P EQUAL 4 )
    SET( LIB_SUFFIX 64 )
    SET( PLATFORM_NAME ${PLATFORM_NAME}64 )
ENDIF( CMAKE_SIZEOF_VOID_P EQUAL 4 )
IF( ${PLATFORM_NAME} STREQUAL osx64 )
    SET( LIB_SUFFIX "" )
ENDIF( ${PLATFORM_NAME} STREQUAL osx64 )

##########################################################################################
# Misc Configuration
# -output directory
SET( CMAKE_ARCHIVE_OUTPUT_DIRECTORY         ${CMAKE_CURRENT_SOURCE_DIR}/dist/lib${LIB_SUFFIX} )
SET( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_CURRENT_SOURCE_DIR}/dist/lib${LIB_SUFFIX} )
SET( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/dist/lib${LIB_SUFFIX} )
SET( CMAKE_LIBRARY_OUTPUT_DIRECTORY         ${CMAKE_CURRENT_SOURCE_DIR}/dist/lib${LIB_SUFFIX} )
SET( CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_CURRENT_SOURCE_DIR}/dist/lib${LIB_SUFFIX} )
SET( CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/dist/lib${LIB_SUFFIX} )
SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY         ${CMAKE_CURRENT_SOURCE_DIR}/dist/bin )
SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_CURRENT_SOURCE_DIR}/dist/bin )
SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/dist/bin )
# -Make sure debug builds are recognized
SET(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "Add a postfix, usually d on windows")
# -Find additional cmake modules
SET( CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR} )
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
# -?
ADD_DEFINITIONS( -DCBSDK_EXPORTS )
IF( WIN32 )
    # From cbhwlib/cbmex.vcproj: PreprocessorDefinitions="WIN32;_WINDOWS;NO_AFX;WINVER=0x0501;CBSDK_EXPORTS;QT_APP"
    ADD_DEFINITIONS( -DWIN32 -D_WINDOWS -DNO_AFX -DWINVER=0x0501 -DNOMINMAX
        -D_CRT_SECURE_NO_WARNINGS
        -D_WINSOCK_DEPRECATED_NO_WARNINGS)  # Not necessary if -DUNICODE
ENDIF( WIN32 )
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
cmake_policy(SET CMP0063 NEW) # ENABLE CMP0063: Honor visibility properties for all target types.
cmake_policy(SET CMP0042 NEW) # ENABLE CMP0042: MACOSX_RPATH is enabled by default.

##########################################################################################
# Third party libraries

# -Qt
#We could use a variable to make this editable by GUI, but then we have to clear Qt4 variables for reconfigure.
#SET(CMAKE_PREFIX_PATH "." CACHE PATH "Qt path. e.g., C:\\Qt\\5.8\\msvc2015_64\\")
SET(USING_QT5 OFF)
find_package(Qt5 COMPONENTS Core)
IF(Qt5_FOUND)
    MESSAGE(STATUS "Found Qt ${Qt5Core_VERSION_STRING}")
    SET(USING_QT5 ON)
    FIND_PACKAGE(Qt5 REQUIRED COMPONENTS Xml Concurrent)
ELSE(Qt5_FOUND)
    FIND_PACKAGE( Qt4 REQUIRED QtCore QtXml)
ENDIF(Qt5_FOUND)
IF(${USING_QT5})
    SET(QT_LIBRARIES Qt5::Core Qt5::Xml Qt5::Concurrent)
ELSE(${USING_QT5})
    INCLUDE( ${QT_USE_FILE} )
ENDIF(${USING_QT5})
ADD_DEFINITIONS( ${QT_DEFINITIONS} -DQT_APP )

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

##########################################################################################
# Files/folders common to multiple targets

SET( LIB_INCL_DIRS
    ${PROJECT_SOURCE_DIR}/cbhwlib
    ${PROJECT_SOURCE_DIR}/Central
    ${PROJECT_SOURCE_DIR}/CentralCommon
)
IF( WIN32 )
    LIST(APPEND LIB_INCL_DIRS "${PROJECT_SOURCE_DIR}/compat/")
ENDIF( WIN32 )

SET( LIB_SOURCE
    ${PROJECT_SOURCE_DIR}/cbmex/cbsdk.cpp
    ${PROJECT_SOURCE_DIR}/cbmex/cbsdk.h
    ${PROJECT_SOURCE_DIR}/cbmex/SdkApp.h
    ${PROJECT_SOURCE_DIR}/cbhwlib/cbhwlib.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/cbhwlib.h
    ${PROJECT_SOURCE_DIR}/cbhwlib/cbHwlibHi.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/cbHwlibHi.h
    ${PROJECT_SOURCE_DIR}/cbhwlib/CCFUtils.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/CCFUtils.h
    ${PROJECT_SOURCE_DIR}/cbhwlib/CCFUtilsBinary.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/CCFUtilsBinary.h
    ${PROJECT_SOURCE_DIR}/cbhwlib/CCFUtilsConcurrent.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/CCFUtilsConcurrent.h
    ${PROJECT_SOURCE_DIR}/cbhwlib/CCFUtilsXml.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/CCFUtilsXml.h
    ${PROJECT_SOURCE_DIR}/cbhwlib/CCFUtilsXmlItems.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/CCFUtilsXmlItems.h
    ${PROJECT_SOURCE_DIR}/cbhwlib/InstNetwork.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/InstNetwork.h
    ${PROJECT_SOURCE_DIR}/cbhwlib/XmlFile.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/XmlFile.h
    ${PROJECT_SOURCE_DIR}/Central/Instrument.cpp
    ${PROJECT_SOURCE_DIR}/Central/Instrument.h
    ${PROJECT_SOURCE_DIR}/Central/UDPsocket.cpp
    ${PROJECT_SOURCE_DIR}/Central/UDPsocket.h
)
IF( WIN32 )
    LIST ( APPEND LIB_SOURCE ${PROJECT_SOURCE_DIR}/cbmex/cbMex.rc )  # ??
ENDIF( WIN32 )

# Qt-MOC headers that need it
SET( LIB_HEADERS
    ${PROJECT_SOURCE_DIR}/cbhwlib/InstNetwork.h
)
IF(${USING_QT5})
    QT5_WRAP_CPP( LIB_HEADERS_MOC ${LIB_HEADERS} )
ELSE(${USING_QT5})
    QT4_WRAP_CPP( LIB_HEADERS_MOC ${LIB_HEADERS} )
ENDIF(${USING_QT5})

# Source for both cbmex and octave targets
SET( LIB_SOURCE_CBMEX
    ${PROJECT_SOURCE_DIR}/cbmex/cbmex.cpp
)

##########################################################################################
# Targets

##
# cbsdk shared / dynamic
ADD_LIBRARY( ${LIB_NAME} SHARED ${LIB_SOURCE} ${LIB_HEADERS_MOC} )
IF (NOT WIN32)
    # Hide unexported symbols
    IF( NOT APPLE )
        SET_TARGET_PROPERTIES( ${LIB_NAME} PROPERTIES LINK_FLAGS "-Wl,--exclude-libs,ALL" )
    ENDIF( NOT APPLE )
ENDIF(NOT WIN32)
TARGET_INCLUDE_DIRECTORIES( ${LIB_NAME} PRIVATE ${LIB_INCL_DIRS})
TARGET_LINK_LIBRARIES( ${LIB_NAME} ${QT_LIBRARIES} )
IF( WIN32 )
    # From cbhw/cbmex.vcproj: AdditionalDependencies="ws2_32.lib winmm.lib libmx.lib libmex.lib libmat.lib QtXmlx644.lib QtCorex644.lib"
    TARGET_LINK_LIBRARIES( ${LIB_NAME} ws2_32 winmm )
ENDIF( WIN32 )

##
# cbsdk_static (optional)
IF(${BUILD_STATIC})
    ADD_LIBRARY( ${LIB_NAME_STATIC} STATIC ${LIB_SOURCE} ${LIB_HEADERS_MOC} )
    TARGET_INCLUDE_DIRECTORIES( ${LIB_NAME_STATIC} PRIVATE ${LIB_INCL_DIRS})
    TARGET_LINK_LIBRARIES( ${LIB_NAME_STATIC} ${QT_LIBRARIES} )
    IF (WIN32 )
        TARGET_LINK_LIBRARIES( ${LIB_NAME_STATIC} ws2_32 winmm )
    ENDIF( WIN32 )
    IF( WIN32 )
        SET_TARGET_PROPERTIES( ${LIB_NAME_STATIC} PROPERTIES COMPILE_FLAGS "-DSTATIC_CBSDK_LINK")
    ELSE( WIN32 )
        # Need relocatable static library
        SET_TARGET_PROPERTIES( ${LIB_NAME_STATIC} PROPERTIES COMPILE_FLAGS "-fPIC")  #-DSTATIC_CBSDK_LINK
        SET_TARGET_PROPERTIES( ${LIB_NAME_STATIC} PROPERTIES LINK_FLAGS "-Wl,--exclude-libs,ALL" )
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
    IF( WIN32 )
        SET ( LIB_SOURCE_CBMEX ${LIB_SOURCE_CBMEX} ${PROJECT_SOURCE_DIR}/cbmex/cbMex.rc )
    ENDIF( WIN32 )
    ADD_LIBRARY( ${LIB_NAME_CBMEX} SHARED ${LIB_SOURCE_CBMEX} )
    
    # Want package name to be cbmex without prefix
    IF( WIN32 )
        # Do not output to Debug/Release directories on Windows
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBMEX} PROPERTIES PREFIX "../" )
        # Manually export mexFunction because __declspec(dllexport) conflicts with its definition in mex.h
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBMEX} PROPERTIES LINK_FLAGS "/EXPORT:mexFunction" )
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
        PRIVATE ${LIB_INCL_DIRS} ${Matlab_INCLUDE_DIRS}
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
    IF( WIN32 )
        SET ( LIB_SOURCE_CBMEX ${LIB_SOURCE_CBMEX} ../cbmex/cbMex.rc )
    ENDIF( WIN32 )
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
        PRIVATE ${LIB_INCL_DIRS} ${OCTAVE_INCLUDE_DIR}
    )
    TARGET_LINK_LIBRARIES( ${LIB_NAME_CBOCT} ${LIB_NAME_STATIC} ${QT_LIBRARIES} ${OCTAVE_LIBRARIES} )
    INSTALL( TARGETS ${LIB_NAME_CBOCT}
        RUNTIME DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink 
        LIBRARY DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink 
        ARCHIVE DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink 
    )
ENDIF( ${BUILD_CBOCT} AND OCTAVE_FOUND )

##
# test
IF(${BUILD_TEST})
    ADD_EXECUTABLE( ${TEST_NAME} ${PROJECT_SOURCE_DIR}/cbmex/testcbsdk.cpp )
    TARGET_INCLUDE_DIRECTORIES( ${TEST_NAME} PRIVATE ${LIB_INCL_DIRS})
    TARGET_LINK_LIBRARIES( ${TEST_NAME} ${LIB_NAME} )
    LIST(APPEND INSTALL_TARGET_LIST ${TEST_NAME})
    IF(WIN32)
        get_target_property(QT5_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
        get_filename_component(QT5_WINDEPLOYQT_EXECUTABLE ${QT5_QMAKE_EXECUTABLE} PATH)
        set(QT5_WINDEPLOYQT_EXECUTABLE "${QT5_WINDEPLOYQT_EXECUTABLE}/windeployqt.exe")
        add_custom_command(TARGET ${TEST_NAME} POST_BUILD
            COMMAND ${QT5_WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:${LIB_NAME}>)
    ENDIF(WIN32)
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
    IF( WIN32 )
        SET ( N2H5_SOURCE ${N2H5_SOURCE} ${PROJECT_SOURCE_DIR}/n2h5/res/n2h5_res.rc )
    ENDIF( WIN32 )
    ADD_EXECUTABLE( ${N2H5_NAME} ${N2H5_SOURCE} )
    TARGET_INCLUDE_DIRECTORIES( ${N2H5_NAME} PRIVATE ${LIB_INCL_DIRS} ${HDF5_INCLUDE_DIRS})
    TARGET_LINK_LIBRARIES (${N2H5_NAME} ${HDF5_LIBRARIES} ${HDF5_HL_LIBRARIES})
    LIST(APPEND INSTALL_TARGET_LIST ${N2H5_NAME})
ENDIF(${BUILD_HDF5} AND HDF5_FOUND )

#########################################################################################
# Install libraries, test executable, and headers
INSTALL( TARGETS ${INSTALL_TARGET_LIST}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib${LIB_SUFFIX}
    ARCHIVE DESTINATION lib${LIB_SUFFIX}
)
INSTALL( FILES ${PROJECT_SOURCE_DIR}/cbmex/cbsdk.h ${PROJECT_SOURCE_DIR}/cbhwlib/cbhwlib.h DESTINATION include )

#########################################################################################
# Shared & Static post-build copy the header files into the dist directory.
ADD_CUSTOM_COMMAND(
    TARGET "${LIB_NAME}"
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/cbmex/cbsdk.h" "${CMAKE_CURRENT_SOURCE_DIR}/dist/include/cbsdk.h"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/cbhwlib/cbhwlib.h" "${CMAKE_CURRENT_SOURCE_DIR}/dist/include/cbhwlib.h"
)
