# CBSDK CMake Build System
# Author: griffin.milsap@gmail.com
# Version 1.0 Dec. 2012
#   Build information for
#       cbsdk Library (static or shared)
#       testcbsdk Test Binary
#
#  MATLAB: First ../Matlab  is looked if mex.h is not found FindMatlab.cmake looks at usual places, 
#   use -DMATLAB_ROOT if installed in unknown location (or copy to ../Matlab)
#  If -DCBMEX_INSTALL_PREFIX can be used to install cbmex to given directory
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
    SET( LIB_SUFFIX "" )
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
#SET( LIB_NAME_CBPY cbpy )
SET( LIB_NAME_CBMEX cbmex )
SET( TEST_NAME testcbsdk )
SET( N2H5_NAME n2h5 )

# Make sure debug builds are recognized
SET(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "Add a postfix, usually d on windows")

FIND_PACKAGE( Qt4 REQUIRED )
#FIND_PACKAGE( PythonLibrary )
#FIND_PACKAGE( NumPy )
FIND_PACKAGE( HDF5 )

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

#SET( LIB_SOURCE_CBPY
#    ../cbmex/cbpy.cpp
#)
SET( LIB_SOURCE_CBMEX
    ../cbmex/cbmex.cpp
)
SET( N2H5_SOURCE
    ../n2h5/main.cpp
    ../n2h5/n2h5.cpp
)


# Handle Qt
QT4_WRAP_CPP( LIB_HEADERS_MOC ${LIB_HEADERS} )
SET( QT_USE_QTXML TRUE )
SET( QT_DONT_USE_QTGUI TRUE )
INCLUDE( ${QT_USE_FILE} )
ADD_DEFINITIONS( ${QT_DEFINITIONS} -DQT_APP )

ADD_DEFINITIONS( -DCBSDK_EXPORTS )

IF( WIN32 )
    ADD_DEFINITIONS( -DWIN32 -D_WINDOWS -DWINVER=0x0501 -DNO_AFX )
    SET ( LIB_SOURCE ${LIB_SOURCE} ../cbmex/cbMex.rc )
    INCLUDE_DIRECTORIES( "../compat/" )
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

# Install headers
set(HEADERS "../cbmex/cbsdk.h" "../cbhwlib/cbhwlib.h")
install(FILES ${HEADERS} DESTINATION include)

ADD_CUSTOM_COMMAND(
    TARGET "${LIB_NAME}"
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${PROJECT_BINARY_DIR}/../cbmex/cbsdk.h" "${PROJECT_BINARY_DIR}/../dist/include/cbsdk.h"
    COMMAND ${CMAKE_COMMAND} -E copy "${PROJECT_BINARY_DIR}/../cbhwlib/cbhwlib.h" "${PROJECT_BINARY_DIR}/../dist/include/cbhwlib.h"
)

#########################################################################################
# Build cbpy only if both NumPy and Python libraries are found 
#IF( NUMPY_FOUND AND PYTHONLIBRARY_FOUND )
#    MESSAGE ( STATUS "Add cbpy build target using Python found at " ${PYTHON_EXECUTABLE})
#    INCLUDE_DIRECTORIES( ${PYTHON_INCLUDE_PATH} ${NUMPY_INCLUDE_DIRS} )
#    IF( WIN32 )
#        SET ( LIB_SOURCE_CBPY ${LIB_SOURCE_CBPY} ../cbmex/cbMex.rc )
#    ENDIF( WIN32 )
#    ADD_LIBRARY( ${LIB_NAME_CBPY} SHARED MODULE ${LIB_SOURCE_CBPY} )
#        
#    # Want package name to be cbpy without prefix
#    IF( WIN32 )
#        # Do not output to Debug/Release directories on Windows
#        SET_TARGET_PROPERTIES( ${LIB_NAME_CBPY} PROPERTIES PREFIX "../" )
#        # cbpy needs special care
#        SET_TARGET_PROPERTIES( ${LIB_NAME_CBPY} PROPERTIES COMPILE_FLAGS "-DCBPYSDK" )
#        # Target is cbpy.pyd
#        SET_TARGET_PROPERTIES( ${LIB_NAME_CBPY} PROPERTIES SUFFIX pyd)
#    ELSE( WIN32 )
#        SET_TARGET_PROPERTIES( ${LIB_NAME_CBPY} PROPERTIES PREFIX "" )
#        # cbpy needs special care and hide unexported symbols
#        IF( NOT APPLE )
#            SET_TARGET_PROPERTIES( ${LIB_NAME} PROPERTIES LINK_FLAGS "-Wl,--exclude-libs,ALL" )
#        ENDIF( NOT APPLE )
#        SET_TARGET_PROPERTIES( ${LIB_NAME_CBPY} PROPERTIES COMPILE_FLAGS "-DCBPYSDK -fvisibility=hidden" )
#    ENDIF( WIN32 )
#    
#    # Use static library to build cbpy
#    ADD_DEPENDENCIES( ${LIB_NAME_CBPY} ${LIB_NAME_STATIC} )
#    TARGET_LINK_LIBRARIES( ${LIB_NAME_CBPY} ${LIB_NAME_STATIC} ${QT_LIBRARIES} ${PYTHON_LIBRARIES} )
#    
#    INSTALL( TARGETS ${LIB_NAME_CBPY}
#        RUNTIME DESTINATION ${PYTHON_SITE_PACKAGES_INSTALL_DIR}/CereLink 
#        LIBRARY DESTINATION ${PYTHON_SITE_PACKAGES_INSTALL_DIR}/CereLink 
#        ARCHIVE DESTINATION ${PYTHON_SITE_PACKAGES_INSTALL_DIR}/CereLink 
#    )
#    INSTALL( FILES ../cbmex/__init__.py DESTINATION ${PYTHON_SITE_PACKAGES_INSTALL_DIR}/CereLink  )
#ENDIF( NUMPY_FOUND AND PYTHONLIBRARY_FOUND )

#########################################################################################
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
    
    SET_TARGET_PROPERTIES( ${LIB_NAME_CBMEX} PROPERTIES SUFFIX .${MEXTENSION})
    IF( NOT CBMEX_INSTALL_PREFIX )
        SET( CBMEX_INSTALL_PREFIX .)
    ENDIF( NOT CBMEX_INSTALL_PREFIX )
    # Use static library to build cbmex
    ADD_DEPENDENCIES( ${LIB_NAME_CBMEX} ${LIB_NAME_STATIC} )
    TARGET_LINK_LIBRARIES( ${LIB_NAME_CBMEX} ${LIB_NAME_STATIC} ${QT_LIBRARIES} ${MATLAB_LIBRARIES} )
    INSTALL( TARGETS ${LIB_NAME_CBMEX}
        RUNTIME DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink 
        LIBRARY DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink 
        ARCHIVE DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink 
    )
ENDIF( MATLAB_FOUND )

#########################################################################################
# Build Test executable
ADD_EXECUTABLE( ${TEST_NAME} ../cbmex/testcbsdk.cpp )
TARGET_LINK_LIBRARIES( ${TEST_NAME} ${LIB_NAME} )

# Install information
INSTALL( TARGETS ${TEST_NAME} ${LIB_NAME} ${LIB_NAME_STATIC}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib${LIB_SUFFIX}
    ARCHIVE DESTINATION lib${LIB_SUFFIX}
)
INSTALL( FILES ../cbmex/cbsdk.h ../cbhwlib/cbhwlib.h DESTINATION include )

#########################################################################################
# Build n2h5 executable only if HDF5 found
IF( HDF5_FOUND )
    MESSAGE ( STATUS "Add n2h5 utility build target")
    # In case it did not find it (seems like a bug in FindHDF5.cmake) 
    IF (NOT HDF5_HL_LIBRARIES)
        MESSAGE( "Couldn't locate HDF5 HL Libraries.  Attempting workaround." )
        FIND_LIBRARY (HDF5_HL_LIBRARIES libhdf5_hl${CMAKE_SHARED_LIBRARY_SUFFIX}
                      HINTS ${HDF5_LIBRARY_DIR}
                      PATHS
                      /usr/lib
                      /usr/lib64
                      /usr/local/lib
                      /usr/local/lib64
                      /opt/local/lib
                      /opt/local/lib64
        )
    ENDIF (NOT HDF5_HL_LIBRARIES)
    MESSAGE( ${HDF5_HL_LIBRARIES} )
    INCLUDE_DIRECTORIES( ${HDF5_INCLUDE_DIR} )
    IF( WIN32 )
        SET ( N2H5_SOURCE ${N2H5_SOURCE} ../n2h5/res/n2h5_res.rc )
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

