# Enable/Disable universal build on Apple Silicon
option(CB_ENABLE_XCODE_UNIVERSAL_BUILD "Enable building x86/arm64 universal binary on Apple Silicon" ON)

# Deployment target/architectures for macOS
if(APPLE)
    # set the deployment target if provided
    if(CB_MACOS_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "${CB_MACOS_DEPLOYMENT_TARGET}" CACHE STRING "")
    endif()

    # on macOS "uname -m" returns the architecture (x86_64 or arm64)
    execute_process(
            COMMAND uname -m
            RESULT_VARIABLE result
            OUTPUT_VARIABLE CB_OSX_NATIVE_ARCHITECTURE
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # determine if we do universal build or native build
    if(CB_ENABLE_XCODE_UNIVERSAL_BUILD                     # is universal build enabled?
            AND (CMAKE_GENERATOR STREQUAL "Xcode")                # works only with Xcode
            AND (CB_OSX_NATIVE_ARCHITECTURE STREQUAL "arm64")) # and only when running on arm64
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
        set(CB_ARCHIVE_ARCHITECTURE "macOS_universal")
        message(STATUS "macOS universal (x86_64 / arm64) build")
    else()
        set(CB_ARCHIVE_ARCHITECTURE "macOS_${CB_OSX_NATIVE_ARCHITECTURE}")
        message(STATUS "macOS native ${CB_OSX_NATIVE_ARCHITECTURE} build")
    endif()
else()
    set(CB_ARCHIVE_ARCHITECTURE "N/A")
endif()