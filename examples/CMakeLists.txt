# Example programs for the modular architecture
# Format: "target_name:SourceDir/source_file.cpp"

# cbsdk examples (link against cbsdk)
set(CBSDK_EXAMPLES
    "gemini_example:GeminiExample/gemini_example.cpp"
    "simple_device:SimpleDevice/simple_device.cpp"
    "central_client:CentralClient/central_client.cpp"
    "ccf_test:CCFTest/ccf_test.cpp"
    "recording_test:RecordingTest/recording_test.cpp"
)

# cbdev examples (link against cbdev only - no cbshm, no cbsdk)
set(CBDEV_EXAMPLES
    "check_protocol_version:CheckProtocolVersion/check_protocol_version.cpp"
    "configure_channels:ConfigureChannels/configure_channels.cpp"
)

set(SAMPLE_TARGET_LIST)

foreach(example ${CBSDK_EXAMPLES})
    string(REPLACE ":" ";" example_parts ${example})
    list(GET example_parts 0 target_name)
    list(GET example_parts 1 source_file)

    add_executable(${target_name} ${source_file})
    target_link_libraries(${target_name} cbsdk)
    list(APPEND SAMPLE_TARGET_LIST ${target_name})
endforeach()

foreach(example ${CBDEV_EXAMPLES})
    string(REPLACE ":" ";" example_parts ${example})
    list(GET example_parts 0 target_name)
    list(GET example_parts 1 source_file)

    add_executable(${target_name} ${source_file})
    target_link_libraries(${target_name} cbdev)
    list(APPEND SAMPLE_TARGET_LIST ${target_name})
endforeach()

# Set RPATH for Unix/macOS
if(NOT CMAKE_INSTALL_RPATH)
    set(LIBDIR "../lib")
    foreach(sample_app ${SAMPLE_TARGET_LIST})
        if(APPLE)
            set_property(TARGET ${sample_app} APPEND
                PROPERTY INSTALL_RPATH "@executable_path/;@executable_path/${LIBDIR};@executable_path/../Frameworks")
        elseif(UNIX)
            set_property(TARGET ${sample_app}
                PROPERTY INSTALL_RPATH "\$ORIGIN:\$ORIGIN/${LIBDIR}")
        endif(APPLE)
    endforeach()
endif(NOT CMAKE_INSTALL_RPATH)

# Export target list to parent scope for installation
set(SAMPLE_TARGET_LIST ${SAMPLE_TARGET_LIST} PARENT_SCOPE)
