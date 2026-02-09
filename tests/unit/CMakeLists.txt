# Unit Tests
# Each module (cbproto, cbshm, cbdev, cbsdk) will have its own test suite

# Only build if new architecture is enabled
if(NOT CBSDK_BUILD_NEW_ARCH)
    message(STATUS "Skipping unit tests for new architecture (CBSDK_BUILD_NEW_ARCH=OFF)")
    return()
endif()

# GoogleTest will be fetched as needed
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

# cbproto tests
add_executable(cbproto_tests
    test_instrument_id.cpp
    test_protocol_structures.cpp
)

target_link_libraries(cbproto_tests
    PRIVATE
        cbproto
        GTest::gtest_main
)

target_include_directories(cbproto_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/cbproto/include
)

gtest_discover_tests(cbproto_tests)

message(STATUS "Unit tests configured for cbproto")

# cbshm tests
add_executable(cbshm_tests
    test_shmem_session.cpp
    test_native_types.cpp
)

target_link_libraries(cbshm_tests
    PRIVATE
        cbshm
        cbproto
        GTest::gtest_main
)

# Include directories inherited from cbshm and cbproto targets

gtest_discover_tests(cbshm_tests)

message(STATUS "Unit tests configured for cbshm")

# cbdev tests
add_executable(cbdev_tests
    test_device_session.cpp
    test_packet_translation.cpp
    packet_test_helpers.cpp
)

target_link_libraries(cbdev_tests
    PRIVATE
        cbdev
        cbproto
        GTest::gtest_main
)

target_include_directories(cbdev_tests
    BEFORE PRIVATE
        ${PROJECT_SOURCE_DIR}/src/cbdev/include
        ${PROJECT_SOURCE_DIR}/src/cbproto/include
)

gtest_discover_tests(cbdev_tests)

message(STATUS "Unit tests configured for cbdev")

# cbsdk tests
add_executable(cbsdk_tests
    test_sdk_session.cpp
    test_sdk_handshake.cpp
    test_cbsdk_c_api.cpp
)

target_link_libraries(cbsdk_tests
    PRIVATE
        cbsdk
        cbdev
        cbproto
        cbshm
        GTest::gtest_main
)

target_include_directories(cbsdk_tests
    BEFORE PRIVATE
        ${PROJECT_SOURCE_DIR}/src/cbsdk/include
        ${PROJECT_SOURCE_DIR}/src/cbdev/include
        ${PROJECT_SOURCE_DIR}/src/cbshm/include
        ${PROJECT_SOURCE_DIR}/src/cbproto/include
        ${PROJECT_SOURCE_DIR}/upstream
)

gtest_discover_tests(cbsdk_tests)

message(STATUS "Unit tests configured for cbsdk")
