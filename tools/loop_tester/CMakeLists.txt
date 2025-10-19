cmake_minimum_required(VERSION 3.15)

# Fetch LSL library from the cboulay/apple_framework branch
# This branch has fixes for macOS framework support
include(FetchContent)
FetchContent_Declare(
    lsl
    GIT_REPOSITORY https://github.com/sccn/liblsl.git
    GIT_TAG cboulay/apple_framework
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(lsl)

# First application -- LSL sawtooth stream source
# Generates a 30 kHz sawtooth signal in first 2 channels and constant values in remaining channels
add_executable(sawtooth_lsl sawtooth_lsl.cpp)
target_link_libraries(sawtooth_lsl PRIVATE lsl)
target_compile_features(sawtooth_lsl PRIVATE cxx_std_17)

# Set RPATH for macOS to find lsl.framework
if(APPLE)
    set_target_properties(sawtooth_lsl PROPERTIES
        INSTALL_RPATH "@executable_path/../lib;@executable_path/../Frameworks"
        BUILD_WITH_INSTALL_RPATH FALSE
        MACOSX_RPATH TRUE
    )
endif()

# Second application -- cbsdk sawtooth validator (trial buffer version)
# Connects to nPlayServer and validates the received sawtooth signal
add_executable(sawtooth_cbsdk sawtooth_cbsdk.cpp)
target_link_libraries(sawtooth_cbsdk PRIVATE cbsdk_static)
target_compile_features(sawtooth_cbsdk PRIVATE cxx_std_17)

# Third application -- cbsdk sawtooth validator (callback version)
# Uses cbSdkRegisterCallback to bypass trial buffer system
add_executable(sawtooth_cbsdk_callback sawtooth_cbsdk_callback.cpp)
target_link_libraries(sawtooth_cbsdk_callback PRIVATE cbsdk_static)
target_compile_features(sawtooth_cbsdk_callback PRIVATE cxx_std_17)

# Install all executables
install(TARGETS sawtooth_lsl sawtooth_cbsdk sawtooth_cbsdk_callback
    RUNTIME DESTINATION bin
)