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

# Second application -- cbsdk sawtooth validator (trial buffer version)
# Connects to nPlayServer and validates the received sawtooth signal
add_executable(cbsdk_sawtooth cbsdk_sawtooth.cpp)
target_link_libraries(cbsdk_sawtooth PRIVATE cbsdk_static)
target_compile_features(cbsdk_sawtooth PRIVATE cxx_std_17)

# Third application -- cbsdk sawtooth validator (callback version)
# Uses cbSdkRegisterCallback to bypass trial buffer system
add_executable(cbsdk_sawtooth_callback cbsdk_sawtooth_callback.cpp)
target_link_libraries(cbsdk_sawtooth_callback PRIVATE cbsdk_static)
target_compile_features(cbsdk_sawtooth_callback PRIVATE cxx_std_17)

# Install all executables
install(TARGETS lsl_sawtooth cbsdk_sawtooth cbsdk_sawtooth_callback
    RUNTIME DESTINATION bin
)