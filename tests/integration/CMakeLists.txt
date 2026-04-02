# Integration Tests
# Tests that require a running nPlayServer instance for real packet exchange.
#
# Note: gated by CBSDK_BUILD_TEST in top-level CMakeLists.txt

# GoogleTest framework (shared with unit tests)
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

###############################################################################
# Download nPlayServer binary and test data from the CereLink GitHub release
###############################################################################

set(CERELINK_RELEASE_URL "https://github.com/CerebusOSS/CereLink/releases/download/v9.3.0")
set(NPLAY_CACHE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_nplay_cache")
file(MAKE_DIRECTORY ${NPLAY_CACHE_DIR})

# --- Detect platform-specific nPlayServer asset ---
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    set(NPLAY_ASSET "nPlayServer-MacOS-arm.zip")
    set(NPLAY_EXE_NAME "nPlayServer")
elseif(WIN32)
    set(NPLAY_ASSET "nPlayServer-Win64.zip")
    set(NPLAY_EXE_NAME "nPlayServer.exe")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
    set(NPLAY_ASSET "nPlayServer-Linux-x64.zip")
    set(NPLAY_EXE_NAME "nPlayServer")
else()
    message(STATUS "Integration tests: no nPlayServer binary for ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}")
    set(NPLAY_ASSET "")
endif()

# --- Download and extract nPlayServer ---
set(NPLAY_AVAILABLE FALSE)

if(NPLAY_ASSET)
    set(NPLAY_ZIP "${NPLAY_CACHE_DIR}/${NPLAY_ASSET}")
    set(NPLAY_EXTRACT_DIR "${NPLAY_CACHE_DIR}/${NPLAY_ASSET}")
    string(REGEX REPLACE "\\.zip$" "" NPLAY_EXTRACT_DIR "${NPLAY_EXTRACT_DIR}")

    if(NOT EXISTS "${NPLAY_ZIP}")
        message(STATUS "Downloading ${NPLAY_ASSET}...")
        file(DOWNLOAD
            "${CERELINK_RELEASE_URL}/${NPLAY_ASSET}"
            "${NPLAY_ZIP}"
            STATUS NPLAY_DL_STATUS
            SHOW_PROGRESS
        )
        list(GET NPLAY_DL_STATUS 0 NPLAY_DL_CODE)
        if(NOT NPLAY_DL_CODE EQUAL 0)
            message(WARNING "Failed to download nPlayServer: ${NPLAY_DL_STATUS}")
            set(NPLAY_ASSET "")
        endif()
    endif()

    if(NPLAY_ASSET AND NOT EXISTS "${NPLAY_EXTRACT_DIR}")
        message(STATUS "Extracting ${NPLAY_ASSET}...")
        file(ARCHIVE_EXTRACT INPUT "${NPLAY_ZIP}" DESTINATION "${NPLAY_EXTRACT_DIR}")
    endif()

    # Find the binary
    if(NPLAY_ASSET)
        file(GLOB_RECURSE NPLAY_BINARY_CANDIDATES "${NPLAY_EXTRACT_DIR}/${NPLAY_EXE_NAME}")
        if(NPLAY_BINARY_CANDIDATES)
            list(GET NPLAY_BINARY_CANDIDATES 0 NPLAY_BINARY)
            # Ensure executable permission on POSIX
            if(NOT WIN32)
                file(CHMOD "${NPLAY_BINARY}" PERMISSIONS
                    OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE)
            endif()
            set(NPLAY_AVAILABLE TRUE)
            message(STATUS "nPlayServer binary: ${NPLAY_BINARY}")
        else()
            message(WARNING "nPlayServer binary not found in extracted archive")
        endif()
    endif()
endif()

# --- Download and extract test data ---
set(NPLAY_DATA_AVAILABLE FALSE)

if(NPLAY_AVAILABLE)
    set(DNSS_ZIP "${NPLAY_CACHE_DIR}/dnss.zip")
    set(DNSS_EXTRACT_DIR "${NPLAY_CACHE_DIR}/dnss")

    if(NOT EXISTS "${DNSS_ZIP}")
        message(STATUS "Downloading test data (dnss.zip)...")
        file(DOWNLOAD
            "${CERELINK_RELEASE_URL}/dnss.zip"
            "${DNSS_ZIP}"
            STATUS DNSS_DL_STATUS
            SHOW_PROGRESS
        )
        list(GET DNSS_DL_STATUS 0 DNSS_DL_CODE)
        if(NOT DNSS_DL_CODE EQUAL 0)
            message(WARNING "Failed to download test data: ${DNSS_DL_STATUS}")
        endif()
    endif()

    if(EXISTS "${DNSS_ZIP}" AND NOT EXISTS "${DNSS_EXTRACT_DIR}")
        message(STATUS "Extracting test data...")
        file(ARCHIVE_EXTRACT INPUT "${DNSS_ZIP}" DESTINATION "${DNSS_EXTRACT_DIR}")
    endif()

    # Find the .ns6 and .ccf files
    if(EXISTS "${DNSS_EXTRACT_DIR}")
        file(GLOB_RECURSE NS6_CANDIDATES "${DNSS_EXTRACT_DIR}/*.ns6")
        file(GLOB_RECURSE CCF_CANDIDATES "${DNSS_EXTRACT_DIR}/*.ccf")
        if(NS6_CANDIDATES)
            list(GET NS6_CANDIDATES 0 NPLAY_NS6_FILE)
            set(NPLAY_DATA_AVAILABLE TRUE)
            message(STATUS "Test data NS6: ${NPLAY_NS6_FILE}")
        endif()
        if(CCF_CANDIDATES)
            list(GET CCF_CANDIDATES 0 NPLAY_CCF_FILE)
            message(STATUS "Test data CCF: ${NPLAY_CCF_FILE}")
        endif()
    endif()
endif()

# CMP file from the repo's test fixtures
set(NPLAY_CMP_FILE "${PROJECT_SOURCE_DIR}/tests/96ChannelDefaultMapping.cmp")

###############################################################################
# Integration test executables
###############################################################################

if(NPLAY_AVAILABLE AND NPLAY_DATA_AVAILABLE)

    # --- C++ SdkSession integration tests ---
    add_executable(cbsdk_integration_tests
        test_sdk_configuration.cpp
    )

    target_link_libraries(cbsdk_integration_tests
        PRIVATE
            cbsdk
            cbdev
            cbproto
            cbshm
            ccfutils
            GTest::gtest_main
    )

    target_include_directories(cbsdk_integration_tests
        BEFORE PRIVATE
            ${PROJECT_SOURCE_DIR}/src/cbsdk/include
            ${PROJECT_SOURCE_DIR}/src/cbdev/include
            ${PROJECT_SOURCE_DIR}/src/cbdev/src
            ${PROJECT_SOURCE_DIR}/src/cbshm/include
            ${PROJECT_SOURCE_DIR}/src/cbproto/include
            ${PROJECT_SOURCE_DIR}/src/ccfutils/include
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_compile_definitions(cbsdk_integration_tests
        PRIVATE
            NPLAY_BINARY_PATH="${NPLAY_BINARY}"
            NPLAY_NS6_PATH="${NPLAY_NS6_FILE}"
            NPLAY_CCF_PATH="${NPLAY_CCF_FILE}"
            NPLAY_CMP_PATH="${NPLAY_CMP_FILE}"
    )

    # Prefix with "integration." so CI can run or skip them independently
    gtest_discover_tests(cbsdk_integration_tests
        TEST_PREFIX "integration."
    )

    message(STATUS "Integration tests configured (prefix: 'integration.')")

    # --- C API integration tests ---
    add_executable(cbsdk_capi_integration_tests
        test_capi_configuration.cpp
    )

    target_link_libraries(cbsdk_capi_integration_tests
        PRIVATE
            cbsdk
            cbdev
            cbproto
            cbshm
            ccfutils
            GTest::gtest_main
    )

    target_include_directories(cbsdk_capi_integration_tests
        BEFORE PRIVATE
            ${PROJECT_SOURCE_DIR}/src/cbsdk/include
            ${PROJECT_SOURCE_DIR}/src/cbdev/include
            ${PROJECT_SOURCE_DIR}/src/cbdev/src
            ${PROJECT_SOURCE_DIR}/src/cbshm/include
            ${PROJECT_SOURCE_DIR}/src/cbproto/include
            ${PROJECT_SOURCE_DIR}/src/ccfutils/include
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_compile_definitions(cbsdk_capi_integration_tests
        PRIVATE
            NPLAY_BINARY_PATH="${NPLAY_BINARY}"
            NPLAY_NS6_PATH="${NPLAY_NS6_FILE}"
            NPLAY_CCF_PATH="${NPLAY_CCF_FILE}"
            NPLAY_CMP_PATH="${NPLAY_CMP_FILE}"
    )

    gtest_discover_tests(cbsdk_capi_integration_tests
        TEST_PREFIX "integration."
    )

    message(STATUS "C API integration tests configured (prefix: 'integration.')")

else()
    message(STATUS "Integration tests SKIPPED: nPlayServer or test data not available")
endif()
