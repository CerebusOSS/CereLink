cmake_minimum_required(VERSION 3.15)

# Fetch LSL library from the cboulay/apple_framework branch
# This branch has fixes for macOS framework support
include(FetchContent)
FetchContent_Declare(
    lsl
    GIT_REPOSITORY https://github.com/sccn/liblsl.git
    GIT_TAG cboulay/apple_framework
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(lsl)

# First application -- LSL sawtooth stream source
# Generates a 30 kHz sawtooth signal in first 2 channels and constant values in remaining channels
add_executable(lsl_sawtooth lsl_sawtooth.cpp)
target_link_libraries(lsl_sawtooth PRIVATE lsl)
target_compile_features(lsl_sawtooth PRIVATE cxx_std_17)
add_executable(sawtooth_lsl sawtooth_lsl.cpp)
target_link_libraries(sawtooth_lsl PRIVATE lsl)
target_compile_features(sawtooth_lsl PRIVATE cxx_std_17)

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