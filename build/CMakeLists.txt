# CBSDK CMake Build System
# Author: griffin.milsap@gmail.com
# Version 1.0 Dec. 2012
#	Build information for
#		cbsdk Library (static or shared)
#       cbpy  Library
#		testcbsdk Test Binary
# Notes:
#  OSX: may need to specify your Python library as cmake may detect system provided PythonLib (although may detect right Python)! 
#    cmake DPYTHON_LIBRARY=/usr/local/Cellar/python/2.7.3/Frameworks/Python.framework/Versions/2.7/lib/libpython2.7.dylib . 
#

CMAKE_MINIMUM_REQUIRED( VERSION 2.8 )

PROJECT( CBSDK )

SET( CBSDK_VERSION_MAJOR 1 )
SET( CBSDK_VERSION_MINOR 0 )

# Handle 64 bit binaries
IF( CMAKE_SIZEOF_VOID_P EQUAL 4 )
	SET( LIB_SUFFIX "" )
ELSE( CMAKE_SIZEOF_VOID_P EQUAL 4 )
	SET( LIB_SUFFIX 64 )
ENDIF( CMAKE_SIZEOF_VOID_P EQUAL 4 )

SET( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/../dist/lib${LIB_SUFFIX} )
SET( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/../dist/lib${LIB_SUFFIX} )
SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/../dist/bin )
SET( CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR} )

SET( LIB_NAME cbsdk )
SET( LIB_NAME_STATIC cbsdk_static )
SET( LIB_NAME_CBPY cbpy )

FIND_PACKAGE( Qt4 REQUIRED )
FIND_PACKAGE( PythonLibs )
FIND_PACKAGE( NumPy )

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

# Handle Qt
QT4_WRAP_CPP( LIB_HEADERS_MOC ${LIB_HEADERS} )
SET( QT_USE_QTXML TRUE )
INCLUDE( ${QT_USE_FILE} )
ADD_DEFINITIONS( ${QT_DEFINITIONS} -DQT_APP )

ADD_DEFINITIONS( -DCBSDK_EXPORTS )

# Make sure debug builds are recognized
SET(CMAKE_DEBUG_POSTFIX "_d")

IF( WIN32 )
	ADD_DEFINITIONS( -DWIN32 -D_WINDOWS -DWINVER=0x0501 -DNO_AFX )
    SET ( LIB_SOURCE ${LIB_SOURCE} ../cbmex/cbMex.rc )
ENDIF( WIN32 )
INCLUDE_DIRECTORIES( ${PYTHON_INCLUDE_DIRS} ${NUMPY_INCLUDE_DIRS} ${MATLAB_INCLUDE_DIRS} )

ADD_LIBRARY( ${LIB_NAME} SHARED ${LIB_SOURCE} ${LIB_HEADERS_MOC} )
ADD_LIBRARY( ${LIB_NAME_STATIC} STATIC ${LIB_SOURCE} ${LIB_HEADERS_MOC} )
# Add static library creation
SET_TARGET_PROPERTIES( ${LIB_NAME_STATIC} PROPERTIES COMPILE_FLAGS "-DSTATIC_CBSDK_LINK")
TARGET_LINK_LIBRARIES( ${LIB_NAME} ${QT_LIBRARIES} )
TARGET_LINK_LIBRARIES( ${LIB_NAME_STATIC} ${QT_LIBRARIES} )

# Build cbpy only of both NumPy and Python are found 
IF( NUMPY_FOUND AND PYTHONLIBS_FOUND )
    MESSAGE ( STATUS "Add cbpy build target using PythonLibs at " ${PYTHON_LIBRARIES})
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

IF( WIN32 )
    # Do not output to Debug/Release directories on Windows
    SET_TARGET_PROPERTIES (${LIB_NAME} PROPERTIES PREFIX "../")
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

