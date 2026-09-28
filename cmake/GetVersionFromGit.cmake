# GetVersionFromGit.cmake
# Function to extract version information from git tags
#
# This function retrieves the most recent git tag following semantic versioning
# and sets the version variables for use in the project() command. When git or a
# matching tag is unavailable (e.g., a source tarball downloaded from GitHub), it
# falls back to the supplied FALLBACK version so the source tree always builds.
#
# Output Variables:
#   GIT_VERSION_MAJOR - Major version number
#   GIT_VERSION_MINOR - Minor version number
#   GIT_VERSION_PATCH - Patch version number
#   GIT_VERSION_FULL  - Full version string (MAJOR.MINOR.PATCH)
#
# Usage:
#   include(GetVersionFromGit)
#   get_version_from_git(FALLBACK "1.2.3")
#   project(MyProject VERSION ${GIT_VERSION_FULL})

function(get_version_from_git)
    cmake_parse_arguments(ARG "" "FALLBACK" "" ${ARGN})
    if(NOT ARG_FALLBACK)
        set(ARG_FALLBACK "0.0.0")
    endif()

    set(VERSION_STRING "")
    # Only query git if this source tree is its own repository, so an extracted
    # tarball nested inside another repo doesn't pick up that repo's tags.
    find_package(Git QUIET)
    if(Git_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_TAG
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE GIT_RESULT
        )
        if(GIT_RESULT EQUAL 0 AND GIT_TAG)
            # Remove 'v' prefix if present (handles both v1.2.3 and 1.2.3)
            string(REGEX REPLACE "^v" "" VERSION_STRING ${GIT_TAG})
            if(NOT VERSION_STRING MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+")
                message(WARNING "Git tag '${GIT_TAG}' does not follow semantic versioning format, using fallback version ${ARG_FALLBACK}")
                set(VERSION_STRING "")
            endif()
        endif()
    endif()

    if(NOT VERSION_STRING)
        if(NOT GIT_TAG)
            message(STATUS "Could not retrieve version from git tags, using fallback version ${ARG_FALLBACK}")
        endif()
        set(VERSION_STRING "${ARG_FALLBACK}")
    endif()

    # Parse semantic version (MAJOR.MINOR.PATCH)
    if(NOT VERSION_STRING MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
        message(FATAL_ERROR "Version '${VERSION_STRING}' is not MAJOR.MINOR.PATCH")
    endif()
    set(GIT_VERSION_MAJOR ${CMAKE_MATCH_1} PARENT_SCOPE)
    set(GIT_VERSION_MINOR ${CMAKE_MATCH_2} PARENT_SCOPE)
    set(GIT_VERSION_PATCH ${CMAKE_MATCH_3} PARENT_SCOPE)
    set(GIT_VERSION_FULL "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}" PARENT_SCOPE)
    message(STATUS "get_version_from_git returned: ${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
endfunction()
