# Define example programs
# Format: "target_name:SourceDir/source_file.cpp"
set(SIMPLE_EXAMPLES
    "simple_ccf:SimpleCCF/simple_ccf.cpp"
    "simple_io:SimpleIO/simple_io.cpp"
    "simple_callback:SimpleIO/simple_callback.cpp"
    "simple_comments:SimpleComments/simple_comments.cpp"
    "simple_analog_out:SimpleAnalogOut/simple_analog_out.cpp"
)

set(SAMPLE_TARGET_LIST)

# Create executables for simple examples (just link against the library)
foreach(example ${SIMPLE_EXAMPLES})
    string(REPLACE ":" ";" example_parts ${example})
    list(GET example_parts 0 target_name)
    list(GET example_parts 1 source_file)

    add_executable(${target_name} ${source_file})
    target_link_libraries(${target_name} ${LIB_NAME})
    list(APPEND SAMPLE_TARGET_LIST ${target_name})
endforeach()

# simple_cbsdk is special - it includes library sources directly for development convenience
# TODO: Consider refactoring to use an object library instead
add_executable(simple_cbsdk
        SimpleCBSDK/simple_cbsdk.cpp
        ${LIB_SOURCE}
)
if(WIN32)
    target_compile_definitions(simple_cbsdk PRIVATE STATIC_CBSDK_LINK)
endif()
target_include_directories(simple_cbsdk PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/cbhwlib
        ${PROJECT_SOURCE_DIR}/src/cbproto
        ${PROJECT_SOURCE_DIR}/src/cbsdk
        ${PROJECT_SOURCE_DIR}/src/ccfutils
        ${PROJECT_SOURCE_DIR}/src/central
        ${PROJECT_SOURCE_DIR}/src/compat
)
target_link_libraries(simple_cbsdk PRIVATE CCFUtils)
if(WIN32)
    target_link_libraries(simple_cbsdk PRIVATE wsock32 ws2_32 winmm)
elseif(NOT APPLE)
    target_link_libraries(simple_cbsdk PRIVATE rt)
    set_target_properties(simple_cbsdk PROPERTIES LINK_FLAGS "-Wl,--exclude-libs,ALL")
endif()
list(APPEND SAMPLE_TARGET_LIST simple_cbsdk)

list(APPEND INSTALL_TARGET_LIST ${SAMPLE_TARGET_LIST})

IF(WIN32)
    # TODO: For each sample, post-build copy required DLLs to the output directory.
    # TODO: For each sample, post-install fixup the install directory with required DLLs.
endif(WIN32)

if(NOT CMAKE_INSTALL_RPATH)
    set(LIBDIR "../lib")
    foreach(test_app ${TEST_SAMPLE_LIST})
        if(APPLE)
            set_property(TARGET ${test_app} APPEND
                    PROPERTY INSTALL_RPATH "@executable_path/;@executable_path/${LIBDIR};@executable_path/../Frameworks")
        elseif(UNIX)
            set_property(TARGET ${test_app}
                    PROPERTY INSTALL_RPATH "\$ORIGIN:\$ORIGIN/${LIBDIR}")
        endif(APPLE)
    endforeach()
endif(NOT CMAKE_INSTALL_RPATH)