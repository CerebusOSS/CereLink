# (use include(CTest) if you want CDash dashboard integration)
if (NOT BUILD_GTEST)
    message(STATUS "Fetching GoogleTest for unit tests")
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.15.2
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(googletest)
endif()

# Unit Tests
add_executable(ccfutils_tests
    CCFUtilsTests.cpp
    test_main.cpp
)

# Synchronization tests (for verifying Phase 2 migration)
add_executable(sync_tests
    SynchronizationTests.cpp
    test_main.cpp
)

# Include necessary headers
target_include_directories(ccfutils_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/CCFUtils
    ${PROJECT_SOURCE_DIR}/cbproto
    ${PROJECT_SOURCE_DIR}/cbsdk
    ${PROJECT_SOURCE_DIR}/cbhwlib
    ${PROJECT_SOURCE_DIR}/Central
    ${PROJECT_SOURCE_DIR}/CentralCommon
    ${PROJECT_SOURCE_DIR}/compat
    ${PROJECT_BINARY_DIR}/_deps/pugixml-src/src/
)

# Link libraries (use gtest + custom main)
target_link_libraries(ccfutils_tests
    PRIVATE
        gtest
        cbsdk_static
        CCFUtils
)

target_compile_definitions(ccfutils_tests PRIVATE PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

add_test(NAME ccfutils_tests COMMAND ccfutils_tests)

# Synchronization tests don't need special includes or links
target_link_libraries(sync_tests
    PRIVATE
        gtest
)

add_test(NAME sync_tests COMMAND sync_tests)
