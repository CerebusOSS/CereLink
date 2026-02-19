# Integration Tests
# Tests cross-module interactions (cbdev + cbshm, cbsdk end-to-end, etc.)

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

# TODO: Add integration test executables when ready
# Examples:
# - cbdev_cbshm_integration: Test device → shmem packet flow
# - cbsdk_standalone_test: End-to-end standalone mode test
# - cbsdk_client_test: End-to-end client mode test (with mock Central)
# - mode_switching_test: Test switching between modes

message(STATUS "Integration test framework configured (waiting for test sources)")
