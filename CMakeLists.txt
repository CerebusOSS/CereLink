# CBSDK CMake Build System
# Author: griffin.milsap@gmail.com
#         chadwick.boulay@gmail.com
# Version 1.1 Jan. 2017
#   Build information for
#       cbsdk Library (static or shared)
#       testcbsdk Test Binary
#
#  MATLAB: First ../Matlab  is looked if mex.h is not found FindMatlab.cmake looks at usual places, 
#   use -DMatlab_ROOT_DIR if installed in unknown location (or copy to ../Matlab)
#  If -DCBMEX_INSTALL_PREFIX can be used to install cbmex to given directory
#
#  QT:
# QT5 ships with its own cmake config files. We need to tell CMake where to find this.
# On Mac OS, assuming qt installed with homebrew,
# run CMake with -DCMAKE_PREFIX_PATH=$(brew --prefix qt5)
# On Windows, set CMAKE_PREFIX_PATH with the Qt path matching your compiler.
# For example, cmake .. -G "Visual Studio 14 2015 Win64" -DCMAKE_PREFIX_PATH="C:\Qt\Qt5.6.0\5.6\msvc2015_64\"


CMAKE_MINIMUM_REQUIRED( VERSION 3.1.0 )

PROJECT( CBSDK )

SET( CBSDK_VERSION_MAJOR 1 )
SET( CBSDK_VERSION_MINOR 0 )

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

SET( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/../dist/lib${LIB_SUFFIX} )
SET( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/../dist/lib${LIB_SUFFIX} )
SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/../dist/bin )
SET( CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR} )

SET( LIB_NAME cbsdk )
SET( LIB_NAME_STATIC cbsdk_static )
SET( LIB_NAME_CBMEX cbmex )
SET( LIB_NAME_CBOCT cboct )
SET( TEST_NAME testcbsdk )
SET( N2H5_NAME n2h5 )

# Make sure debug builds are recognized
SET(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "Add a postfix, usually d on windows")

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

## QT ##
#
#We could use a variable to make this editable by GUI, but then we have to clear Qt4 variables for reconfigure.
#SET(CMAKE_PREFIX_PATH "." CACHE PATH "Qt path. e.g., C:\\Qt\\Qt5.6.0\\5.6\\msvc2015\\")
SET(USING_QT5 OFF)
FIND_PACKAGE( Qt5Core QUIET )
IF(Qt5Core_FOUND)
    MESSAGE(STATUS "Found Qt ${Qt5Core_VERSION_STRING}")
    SET(USING_QT5 ON)
    FIND_PACKAGE( Qt5Xml REQUIRED )
    FIND_PACKAGE( Qt5Concurrent REQUIRED )
ELSE(Qt5Core_FOUND)
    FIND_PACKAGE( Qt4 REQUIRED QtCore QtXml)
ENDIF(Qt5Core_FOUND)
#FIND_PACKAGE( PythonLibrary )
#FIND_PACKAGE( NumPy )
FIND_PACKAGE( HDF5 COMPONENTS C HL)

# Try MATLAB locally first, then on MATLAB install
FIND_PATH( Matlab_INCLUDE_DIRS
    "mex.h"
    "${PROJECT_SOURCE_DIR}/Matlab/include"
)
IF ( Matlab_INCLUDE_DIRS )
    SET( MATLAB_ROOT "${PROJECT_SOURCE_DIR}/Matlab" )
    MESSAGE ( STATUS "Search mex libraries at " ${Matlab_INCLUDE_DIRS}/../lib/${PLATFORM_NAME} )
    FILE( GLOB_RECURSE MATLAB_LIBRARIES ${Matlab_INCLUDE_DIRS}/../lib/${PLATFORM_NAME}/libm*.* )
    IF( MATLAB_LIBRARIES )
        SET( MATLAB_FOUND 1 )
    ENDIF( MATLAB_LIBRARIES )
ELSE ( Matlab_INCLUDE_DIRS )
    #SET( MATLAB_FIND_DEBUG 1 )
    FIND_PACKAGE( Matlab COMPONENTS MX_LIBRARY)
    #On my Mac OS it didn’t search correctly without -DMatlab_ROOT_DIR=/Applications/MATLAB_R2016a.app/
ENDIF ( Matlab_INCLUDE_DIRS )

FIND_PACKAGE( Octave )

INCLUDE_DIRECTORIES(
    ${PROJECT_SOURCE_DIR}/cbhwlib
    ${PROJECT_SOURCE_DIR}/Central
    ${PROJECT_SOURCE_DIR}/Central Common
)

SET( LIB_SOURCE
    ${PROJECT_SOURCE_DIR}/cbmex/cbsdk.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/cbhwlib.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/cbHwlibHi.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/CCFUtils.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/CCFUtilsBinary.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/CCFUtilsConcurrent.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/CCFUtilsXml.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/CCFUtilsXmlItems.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/InstNetwork.cpp
    ${PROJECT_SOURCE_DIR}/cbhwlib/XmlFile.cpp
    ${PROJECT_SOURCE_DIR}/Central/Instrument.cpp
    ${PROJECT_SOURCE_DIR}/Central/UDPsocket.cpp
)

# Only headers which need MOC'ing
SET( LIB_HEADERS
    ${PROJECT_SOURCE_DIR}/cbhwlib/InstNetwork.h
)


SET( LIB_SOURCE_CBMEX
    ${PROJECT_SOURCE_DIR}/cbmex/cbmex.cpp
)
SET( N2H5_SOURCE
    ${PROJECT_SOURCE_DIR}/n2h5/main.cpp
    ${PROJECT_SOURCE_DIR}/n2h5/n2h5.cpp
)


# Handle Qt
IF(${USING_QT5})
    QT5_WRAP_CPP( LIB_HEADERS_MOC ${LIB_HEADERS} )
    SET(QT_LIBRARIES Qt5::Core Qt5::Xml Qt5::Concurrent)
ELSE(${USING_QT5})
    QT4_WRAP_CPP( LIB_HEADERS_MOC ${LIB_HEADERS} )
    INCLUDE( ${QT_USE_FILE} )
ENDIF(${USING_QT5})
ADD_DEFINITIONS( ${QT_DEFINITIONS} -DQT_APP )


ADD_DEFINITIONS( -DCBSDK_EXPORTS )

IF( WIN32 )
    ADD_DEFINITIONS( -DWIN32 -D_WINDOWS -DWINVER=0x0501 -DNO_AFX )
    SET ( LIB_SOURCE ${LIB_SOURCE} ${PROJECT_SOURCE_DIR}/cbmex/cbMex.rc )
    INCLUDE_DIRECTORIES( "${PROJECT_SOURCE_DIR}/compat/" )
ENDIF( WIN32 )

#########################################################################################
# Build cbsdk and cbsdk_static
ADD_LIBRARY( ${LIB_NAME} SHARED ${LIB_SOURCE} ${LIB_HEADERS_MOC} )
ADD_LIBRARY( ${LIB_NAME_STATIC} STATIC ${LIB_SOURCE} ${LIB_HEADERS_MOC} )
IF( WIN32 )
    SET_TARGET_PROPERTIES( ${LIB_NAME_STATIC} PROPERTIES COMPILE_FLAGS "-DSTATIC_CBSDK_LINK")
ELSE( WIN32 )
    # Need relocatable static library
    SET_TARGET_PROPERTIES( ${LIB_NAME_STATIC} PROPERTIES COMPILE_FLAGS "-DSTATIC_CBSDK_LINK -fPIC")
    # Hide unexported symbols
    IF( NOT APPLE )
        SET_TARGET_PROPERTIES( ${LIB_NAME} PROPERTIES LINK_FLAGS "-Wl,--exclude-libs,ALL" )
    ENDIF( NOT APPLE )
    SET_TARGET_PROPERTIES( ${LIB_NAME} PROPERTIES COMPILE_FLAGS "-fvisibility=hidden" )
ENDIF( WIN32)

TARGET_LINK_LIBRARIES( ${LIB_NAME} ${QT_LIBRARIES} )
TARGET_LINK_LIBRARIES( ${LIB_NAME_STATIC} ${QT_LIBRARIES} )


IF( WIN32 )
    # Do not output to Debug/Release directories on Windows
    SET_TARGET_PROPERTIES (${LIB_NAME} PROPERTIES PREFIX "../")
    SET_TARGET_PROPERTIES( ${LIB_NAME} PROPERTIES IMPORT_PREFIX "../")
    SET_TARGET_PROPERTIES (${LIB_NAME_STATIC} PROPERTIES PREFIX "../")

    TARGET_LINK_LIBRARIES( ${LIB_NAME} ws2_32 winmm )
    TARGET_LINK_LIBRARIES( ${LIB_NAME_STATIC} ws2_32 winmm )
ENDIF( WIN32 )

ADD_CUSTOM_COMMAND(
    TARGET "${LIB_NAME}"
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${PROJECT_BINARY_DIR}/../cbmex/cbsdk.h" "${PROJECT_BINARY_DIR}/../dist/include/cbsdk.h"
    COMMAND ${CMAKE_COMMAND} -E copy "${PROJECT_BINARY_DIR}/../cbhwlib/cbhwlib.h" "${PROJECT_BINARY_DIR}/../dist/include/cbhwlib.h"
)

#########################################################################################
# Build cbmex if MATLAB found 
IF( MATLAB_FOUND )
    MESSAGE ( STATUS "Add cbmex build target using MATLAB libs at " ${Matlab_ROOT_DIR})
    INCLUDE_DIRECTORIES( ${Matlab_INCLUDE_DIRS} )
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
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBMEX} PROPERTIES COMPILE_FLAGS "-fvisibility=hidden" )
        # This is for normal users of MATLAB on OSX without homebrew 
        #  so we try to use relative paths to be able to bundle shared libraries 
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBMEX} PROPERTIES BUILD_WITH_INSTALL_RPATH 1 INSTALL_NAME_DIR "@rpath")
    ELSE( WIN32 )
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBMEX} PROPERTIES PREFIX "" )
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBMEX} PROPERTIES LINK_FLAGS "-Wl,--exclude-libs,ALL" )
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBMEX} PROPERTIES COMPILE_FLAGS "-fvisibility=hidden" )
    ENDIF( WIN32 )
    
    SET_TARGET_PROPERTIES( ${LIB_NAME_CBMEX} PROPERTIES SUFFIX .${Matlab_MEX_EXTENSION})
    IF( NOT CBMEX_INSTALL_PREFIX )
        SET( CBMEX_INSTALL_PREFIX .)
    ENDIF( NOT CBMEX_INSTALL_PREFIX )
    # Use static library to build cbmex
    ADD_DEPENDENCIES( ${LIB_NAME_CBMEX} ${LIB_NAME_STATIC} )
    TARGET_LINK_LIBRARIES( ${LIB_NAME_CBMEX} ${LIB_NAME_STATIC} ${QT_LIBRARIES} ${Matlab_LIBRARIES} )
    INSTALL( TARGETS ${LIB_NAME_CBMEX}
        RUNTIME DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink 
        LIBRARY DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink 
        ARCHIVE DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink 
    )
ENDIF( MATLAB_FOUND )

#########################################################################################
# Build cbmex if Octave found 
IF( OCTAVE_FOUND )

    MESSAGE ( STATUS "Add cbmex build target using Octave libs at " ${OCTAVE_OCT_LIB_DIR})
    INCLUDE_DIRECTORIES( ${OCTAVE_INCLUDE_DIR} )
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
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBOCT} PROPERTIES COMPILE_FLAGS "-fvisibility=hidden" )
        # This is for normal users of MATLAB on OSX without homebrew 
        #  so we try to use relative paths to be able to bundle shared libraries 
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBOCT} PROPERTIES BUILD_WITH_INSTALL_RPATH 1 INSTALL_NAME_DIR "@rpath")
    ELSE( WIN32 )
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBOCT} PROPERTIES PREFIX "" )
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBOCT} PROPERTIES LINK_FLAGS "-Wl,--exclude-libs,ALL" )
        SET_TARGET_PROPERTIES( ${LIB_NAME_CBOCT} PROPERTIES COMPILE_FLAGS "-fvisibility=hidden" )
    ENDIF( WIN32 )
    
    SET_TARGET_PROPERTIES( ${LIB_NAME_CBOCT} PROPERTIES SUFFIX .mex)
    IF( NOT CBMEX_INSTALL_PREFIX )
        SET( CBMEX_INSTALL_PREFIX .)
    ENDIF( NOT CBMEX_INSTALL_PREFIX )
    # Use static library to build cbmex
    ADD_DEPENDENCIES( ${LIB_NAME_CBOCT} ${LIB_NAME_STATIC} )
    TARGET_LINK_LIBRARIES( ${LIB_NAME_CBOCT} ${LIB_NAME_STATIC} ${QT_LIBRARIES} ${OCTAVE_LIBRARIES} )
    INSTALL( TARGETS ${LIB_NAME_CBOCT}
        RUNTIME DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink 
        LIBRARY DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink 
        ARCHIVE DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink 
    )
ENDIF( OCTAVE_FOUND )

#########################################################################################
# Build Test executable
ADD_EXECUTABLE( ${TEST_NAME} ${PROJECT_SOURCE_DIR}/cbmex/testcbsdk.cpp )
TARGET_LINK_LIBRARIES( ${TEST_NAME} ${LIB_NAME} )

# Install libraries and test executable
INSTALL( TARGETS ${TEST_NAME} ${LIB_NAME} ${LIB_NAME_STATIC}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib${LIB_SUFFIX}
    ARCHIVE DESTINATION lib${LIB_SUFFIX}
)
# Install headers
INSTALL( FILES ${PROJECT_SOURCE_DIR}/cbmex/cbsdk.h ${PROJECT_SOURCE_DIR}/cbhwlib/cbhwlib.h DESTINATION include )




#########################################################################################
# Build n2h5 executable only if HDF5 found
IF( HDF5_FOUND )
    MESSAGE ( STATUS "Add n2h5 utility build target")
    INCLUDE_DIRECTORIES( ${HDF5_INCLUDE_DIRS} )
    IF( WIN32 )
        SET ( N2H5_SOURCE ${N2H5_SOURCE} ${PROJECT_SOURCE_DIR}/n2h5/res/n2h5_res.rc )
    ENDIF( WIN32 )
    ADD_EXECUTABLE( ${N2H5_NAME} ${N2H5_SOURCE} )
    TARGET_LINK_LIBRARIES (${N2H5_NAME} ${HDF5_LIBRARIES} ${HDF5_HL_LIBRARIES})
    # Install information
    INSTALL( TARGETS ${N2H5_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib${LIB_SUFFIX}
        ARCHIVE DESTINATION lib${LIB_SUFFIX}
    )
ENDIF( HDF5_FOUND )

