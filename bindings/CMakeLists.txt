# Language Wrappers CMake Build System
# This file contains build configuration for all language wrappers:
#   - MATLAB (cbmex)
#   - Octave (cboct)
#   - C++/CLI

##########################################################################################
# Find language-specific dependencies

# -Matlab
IF(${CBSDK_BUILD_CBMEX})
    # Try MATLAB locally first, then on MATLAB install
    FIND_PATH( Matlab_INCLUDE_DIRS
        "mex.h"
        "${PROJECT_SOURCE_DIR}/wrappers/Matlab/include"
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

        SET( MATLAB_ROOT "${PROJECT_SOURCE_DIR}/wrappers/Matlab" )
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
IF(${CBSDK_BUILD_CBOCT})
    FIND_PACKAGE( Octave )
ENDIF()

##########################################################################################
# Source for both cbmex and octave targets
SET( LIB_SOURCE_CBMEX
    ${PROJECT_SOURCE_DIR}/wrappers/cbmex/cbmex.cpp
)
IF( MSVC )
    LIST ( APPEND LIB_SOURCE_CBMEX ${PROJECT_SOURCE_DIR}/wrappers/cbmex/cbMex.rc )
ENDIF( MSVC )

##########################################################################################
# cbmex target
IF(${CBSDK_BUILD_CBMEX} AND MATLAB_FOUND )
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
        ${Matlab_LIBRARIES}
    )
    INSTALL( TARGETS ${LIB_NAME_CBMEX}
        RUNTIME DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink
        LIBRARY DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink
        ARCHIVE DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink
    )
ENDIF( ${CBSDK_BUILD_CBMEX} AND MATLAB_FOUND )

##########################################################################################
# octave target
IF( ${CBSDK_BUILD_CBOCT} AND OCTAVE_FOUND )
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
    TARGET_LINK_LIBRARIES( ${LIB_NAME_CBOCT} ${LIB_NAME_STATIC} ${OCTAVE_LIBRARIES} )
    INSTALL( TARGETS ${LIB_NAME_CBOCT}
        RUNTIME DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink
        LIBRARY DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink
        ARCHIVE DESTINATION ${CBMEX_INSTALL_PREFIX}/CereLink
    )
ENDIF( ${CBSDK_BUILD_CBOCT} AND OCTAVE_FOUND )

##########################################################################################
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
