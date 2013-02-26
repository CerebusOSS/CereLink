# CBSDK CMake Build System
# Author: griffin.milsap@gmail.com
# Version 1.0 Dec. 2012
#	Build information for
#		cbsdk Library (static or shared)
#       cbpy  Library
#		testcbsdk Test Binary
# Notes:
#  OSX: may need to specify your Python library as cmake may detect system provided PythonLib (although may detect right Python)! 
#    e.g. cmake -DPYTHON_LIBRARY=/usr/local/Cellar/python/2.7.3/Frameworks/Python.framework/Versions/2.7/lib/libpython2.7.dylib . 
#
#  MATLAB: First ../Matlab  is looked if mex.h is not found FindMatlab.cmake looks at usual places, 
#   use -DMATLAB_ROOT if installed in unknown location (or copy to ../Matlab)
#

CMAKE_MINIMUM_REQUIRED( VERSION 2.8 )

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

# Decide platform specific distinctions here
IF( ${PLATFORM_NAME} STREQUAL win32 )
    SET( MEXTENSION "mexw32" )
ELSEIF( ${PLATFORM_NAME} STREQUAL win64 )
    SET( MEXTENSION "mexw64" )
ELSEIF( ${PLATFORM_NAME} STREQUAL linux32 )
    SET( MEXTENSION "mexglx" )
ELSEIF( ${PLATFORM_NAME} STREQUAL linux64 )
    SET( MEXTENSION "mexa64" )
ELSEIF( ${PLATFORM_NAME} STREQUAL osx64 )
    SET( MEXTENSION "mexmaci64" )
ELSE( ${PLATFORM_NAME} STREQUAL win32 )
    MESSAGE( STATUS "Platform not supported for cbmex" )
    SET( MEXTENSION "" )
ENDIF( ${PLATFORM_NAME} STREQUAL win32 )

SET( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/../dist/lib${LIB_SUFFIX} )
SET( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/../dist/lib${LIB_SUFFIX} )
SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/../dist/bin )
SET( CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR} )

SET( LIB_NAME cbsdk )
SET( LIB_NAME_STATIC cbsdk_static )
SET( LIB_NAME_CBPY cbpy )
SET( LIB_NAME_CBMEX cbmex )

# Make sure debug builds are recognized
SET(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "Add a postfix, usually d on windows")

FIND_PACKAGE( Qt4 REQUIRED )
FIND_PACKAGE( PythonLibs )
FIND_PACKAGE( NumPy )

# Try MATLAB locally first, then on MATLAB install
FIND_PATH( MATLAB_INCLUDE_DIR
    "mex.h"
    "../Matlab/include"
)
IF ( MATLAB_INCLUDE_DIR-NOTFOUND )
    FIND_PACKAGE( Matlab )
ELSE ( MATLAB_INCLUDE_DIR-NOTFOUND )
    SET( MATLAB_ROOT "../Matlab" )
    MESSAGE ( STATUS "Search mex libraries at " ${MATLAB_INCLUDE_DIR}/../lib/${PLATFORM_NAME} )
    FILE( GLOB_RECURSE MATLAB_LIBRARIES ${MATLAB_INCLUDE_DIR}/../lib/${PLATFORM_NAME}/libm*.* )
    IF( MATLAB_LIBRARIES )
        SET( MATLAB_FOUND 1 )
    ENDIF( MATLAB_LIBRARIES )
ENDIF ( MATLAB_INCLUDE_DIR-NOTFOUND )

INCLUDE_DIRECTORIES(
	../cbhwlib
	../Central
	../Central Common
)

SET( LIB_SOURCE
	../cbmex/cbsdk.cpp
	../cbhwlib/cbhwlib.cpp
	../cbhwlib/cbHwlibHi.cpp
	../cbhwlib/CCFUtils.cpp
	../cbhwlib/CCFUtilsBinary.cpp
	../cbhwlib/CCFUtilsConcurrent.cpp
	../cbhwlib/CCFUtilsXml.cpp
	../cbhwlib/CCFUtilsXmlItems.cpp
	../cbhwlib/InstNetwork.cpp
	../cbhwlib/XmlFile.cpp
	../Central/Instrument.cpp
	../Central/UDPsocket.cpp
)

# Only headers which need MOC'ing
SET( LIB_HEADERS
	../cbhwlib/InstNetwork.h
)

SET( LIB_SOURCE_CBPY
	../cbmex/cbpy.cpp
)
SET( LIB_SOURCE_CBMEX
	../cbmex/cbmex.cpp
)

# Handle Qt
QT4_WRAP_CPP( LIB_HEADERS_MOC ${LIB_HEADERS} )
SET( QT_USE_QTXML TRUE )
INCLUDE( ${QT_USE_FILE} )
ADD_DEFINITIONS( ${QT_DEFINITIONS} -DQT_APP )

ADD_DEFINITIONS( -DCBSDK_EXPORTS )

IF( WIN32 )
	ADD_DEFINITIONS( -DWIN32 -D_WINDOWS -DWINVER=0x0501 -DNO_AFX )
    SET ( LIB_SOURCE ${LIB_SOURCE} ../cbmex/cbMex.rc )
ENDIF( WIN32 )

ADD_LIBRARY( ${LIB_NAME} SHARED ${LIB_SOURCE} ${LIB_HEADERS_MOC} )
ADD_LIBRARY( ${LIB_NAME_STATIC} STATIC ${LIB_SOURCE} ${LIB_HEADERS_MOC} )
# Add static library creation
SET_TARGET_PROPERTIES( ${LIB_NAME_STATIC} PROPERTIES COMPILE_FLAGS "-DSTATIC_CBSDK_LINK")
TARGET_LINK_LIBRARIES( ${LIB_NAME} ${QT_LIBRARIES} )
TARGET_LINK_LIBRARIES( ${LIB_NAME_STATIC} ${QT_LIBRARIES} )

# Build cbpy only if both NumPy and Python libraries are found 
IF( NUMPY_FOUND AND PYTHONLIBS_FOUND )
    MESSAGE ( STATUS "Add cbpy build target using PythonLibs at " ${PYTHON_LIBRARIES})
    INCLUDE_DIRECTORIES( ${PYTHON_INCLUDE_DIRS} ${NUMPY_INCLUDE_DIRS} )
IF( WIN32 )
    SET ( LIB_SOURCE_CBPY ${LIB_SOURCE_CBPY} ../cbmex/cbMex.rc )
ENDIF( WIN32 )
    ADD_LIBRARY( ${LIB_NAME_CBPY} SHARED MODULE ${LIB_SOURCE_CBPY} )
    SET( CBPY_PACKAGE_PATH ${PROJECT_BINARY_DIR}/../dist/lib${LIB_SUFFIX}/CereLink )
    # cbpy needs special care
    SET_TARGET_PROPERTIES( ${LIB_NAME_CBPY} PROPERTIES COMPILE_FLAGS "-DCBPYSDK" )
    
    # Want package name to be cbpy without prefix
IF( WIN32 )
    # Do not output to Debug/Release directories on Windows
    SET_TARGET_PROPERTIES( ${LIB_NAME_CBPY} PROPERTIES PREFIX "../" )
ELSE( WIN32 )
    SET_TARGET_PROPERTIES( ${LIB_NAME_CBPY} PROPERTIES PREFIX "" )
ENDIF( WIN32 )

    SET_TARGET_PROPERTIES( ${LIB_NAME_CBPY} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CBPY_PACKAGE_PATH} )
    # Use static library to build cbpy
    ADD_DEPENDENCIES( ${LIB_NAME_CBPY} ${LIB_NAME_STATIC} )
    TARGET_LINK_LIBRARIES( ${LIB_NAME_CBPY} ${LIB_NAME_STATIC} ${QT_LIBRARIES} ${PYTHON_LIBRARIES} )
    ADD_CUSTOM_COMMAND(
        TARGET "${LIB_NAME_CBPY}"
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${PROJECT_BINARY_DIR}/../cbmex/__init__.py" "${CBPY_PACKAGE_PATH}/__init__.py"
    )
ENDIF( NUMPY_FOUND AND PYTHONLIBS_FOUND )

# Build cbmex only if MATLAB found 
IF( MATLAB_FOUND )
    MESSAGE ( STATUS "Add cbmex build target using MATLAB libs at " ${MATLAB_ROOT})
    INCLUDE_DIRECTORIES( ${MATLAB_INCLUDE_DIR} )
IF( WIN32 )
    SET ( LIB_SOURCE_CBMEX ${LIB_SOURCE_CBMEX} ../cbmex/cbMex.rc )
ENDIF( WIN32 )
    ADD_LIBRARY( ${LIB_NAME_CBMEX} SHARED ${LIB_SOURCE_CBMEX} )
    
    # Want package name to be cbmex without prefix
IF( WIN32 )
    # Do not output to Debug/Release directories on Windows
    SET_TARGET_PROPERTIES( ${LIB_NAME_CBMEX} PROPERTIES PREFIX "../" )
    # Manually export mexFunction because __declspec(dllexport) conflicts with its definition in mex.h
    SET_TARGET_PROPERTIES( ${LIB_NAME_CBMEX} PROPERTIES LINK_FLAGS "/EXPORT:mexFunction" )
ELSE( WIN32 )
    SET_TARGET_PROPERTIES( ${LIB_NAME_CBMEX} PROPERTIES PREFIX "" )
ENDIF( WIN32 )

    # Use static library to build cbmex
    ADD_DEPENDENCIES( ${LIB_NAME_CBMEX} ${LIB_NAME_STATIC} )
    TARGET_LINK_LIBRARIES( ${LIB_NAME_CBMEX} ${LIB_NAME_STATIC} ${QT_LIBRARIES} ${MATLAB_LIBRARIES} )
    GET_TARGET_PROPERTY( MEX_FILE ${LIB_NAME_CBMEX} LOCATION)
    ADD_CUSTOM_COMMAND(
        TARGET "${LIB_NAME_CBMEX}"
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${MEX_FILE}" "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${LIB_NAME_CBMEX}.${MEXTENSION}"
    )
ENDIF( MATLAB_FOUND )

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

# Test executable
SET( TEST_NAME testcbsdk )
ADD_EXECUTABLE( ${TEST_NAME} ../cbmex/testcbsdk.cpp )
TARGET_LINK_LIBRARIES( ${TEST_NAME} ${LIB_NAME} )

# Install information
INSTALL( TARGETS ${TEST_NAME} ${LIB_NAME}
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib${LIB_SUFFIX}
	ARCHIVE DESTINATION lib${LIB_SUFFIX}
)
INSTALL( FILES ../cbmex/cbsdk.h ../cbhwlib/cbhwlib.h DESTINATION include )

