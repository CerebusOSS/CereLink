# Define example programs
# Format: "target_name:SourceDir/source_file.cpp"
set(SIMPLE_EXAMPLES
    "simple_cbsdk:SimpleCBSDK/simple_cbsdk.cpp"
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
    target_link_libraries(${target_name} ${LIB_NAME_STATIC})
    list(APPEND SAMPLE_TARGET_LIST ${target_name})
endforeach()

list(APPEND INSTALL_TARGET_LIST ${SAMPLE_TARGET_LIST})

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