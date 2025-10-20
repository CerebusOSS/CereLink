option(CBSDK_BUILD_CLI "Build C++/CLI" OFF)

IF(${CBSDK_BUILD_CLI})
    cmake_minimum_required(VERSION 3.12)
    set(CMAKE_CSharp_FLAGS "/langversion:latest")
    SET( LIB_NAME_CLI cbsdk_cli)
    SET(cbsdk_cli_SOURCE
        ${CMAKE_CURRENT_LIST_DIR}/cbsdk_native.h
        ${CMAKE_CURRENT_LIST_DIR}/cbsdk_native.cpp
        ${CMAKE_CURRENT_LIST_DIR}/AssemblyInfo.cpp)
    set_source_files_properties(${cbsdk_cli_SOURCE} PROPERTIES LANGUAGE "CXX")
    ADD_LIBRARY(${LIB_NAME_CLI} MODULE ${cbsdk_cli_SOURCE})
    TARGET_INCLUDE_DIRECTORIES(${LIB_NAME_CLI} PRIVATE
        ${LIB_INCL_DIRS}
        ${PROJECT_SOURCE_DIR}/wrappers/cbmex)
    # Link to the native library.
    TARGET_LINK_LIBRARIES( ${LIB_NAME_CLI}
        ${LIB_NAME}
        # ${QT_LIBRARIES}
    )
    ADD_DEPENDENCIES(${LIB_NAME_CLI} ${LIB_NAME})
    # Set to CLR
    SET_TARGET_PROPERTIES(${LIB_NAME_CLI}
        PROPERTIES
            COMMON_LANGUAGE_RUNTIME ""
            #DEBUG_POSTFIX ""  # With d postfix then the C# app can't find it.
    )
    # target_compile_options(${LIB_NAME_CLI} PRIVATE /clr)
    # target_compile_options(${LIB_NAME_CLI} PRIVATE /EHa)
    # STRING(REPLACE "/EHsc" "/EHa" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    # STRING (REGEX REPLACE "[/|-]RTC(su|[1su])" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    # STRING (REGEX REPLACE "[/|-]RTC(su|[1su])" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    LIST(APPEND INSTALL_TARGET_LIST ${LIB_NAME_CLI})
    
    # CSharp program to TestCLI
    add_subdirectory(TestCLI)
    
    # Create an extended version of the library with a simple C++ wrapper class.
    SET(LIB_NAME_EXT cbsdk_ext)
    ADD_LIBRARY(${LIB_NAME_EXT} SHARED
        ${CMAKE_CURRENT_LIST_DIR}/cbsdk_native.h
        ${CMAKE_CURRENT_LIST_DIR}/cbsdk_native.cpp
        )
    TARGET_INCLUDE_DIRECTORIES(${LIB_NAME_EXT} PRIVATE ${LIB_INCL_DIRS} ${CMAKE_SOURCE_DIR}/wrappers/cbmex)
    TARGET_LINK_LIBRARIES(${LIB_NAME_EXT} ${LIB_NAME})
    ADD_DEPENDENCIES(${LIB_NAME_EXT} ${LIB_NAME})
    LIST(APPEND INSTALL_TARGET_LIST ${LIB_NAME_EXT})
    
    # CSharp program to test cbsdk_ext
    add_subdirectory(TestCSharp)

ENDIF(${CBSDK_BUILD_CLI})