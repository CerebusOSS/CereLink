# GetVersionFromGit.cmake
# Function to extract version information from git tags
#
# This function retrieves the most recent git tag following semantic versioning
# and sets the version variables for use in the project() command.
#
# Output Variables:
#   GIT_VERSION_MAJOR - Major version number
#   GIT_VERSION_MINOR - Minor version number
#   GIT_VERSION_PATCH - Patch version number
#   GIT_VERSION_FULL  - Full version string (MAJOR.MINOR.PATCH)
#
# Usage:
#   include(GetVersionFromGit)
#   get_version_from_git()
#   project(MyProject VERSION ${GIT_VERSION_FULL})

function(get_version_from_git)
    # Get the latest git tag
    execute_process(
        COMMAND git describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GIT_RESULT
    )

    # Check if git command succeeded and we got a tag
    if(GIT_RESULT EQUAL 0 AND GIT_TAG)
        # Remove 'v' prefix if present (handles both v1.2.3 and 1.2.3)
        string(REGEX REPLACE "^v" "" VERSION_STRING ${GIT_TAG})

        # Parse semantic version (MAJOR.MINOR.PATCH)
        if(VERSION_STRING MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
            set(GIT_VERSION_MAJOR ${CMAKE_MATCH_1} PARENT_SCOPE)
            set(GIT_VERSION_MINOR ${CMAKE_MATCH_2} PARENT_SCOPE)
            set(GIT_VERSION_PATCH ${CMAKE_MATCH_3} PARENT_SCOPE)
            set(GIT_VERSION_FULL "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}" PARENT_SCOPE)
            message(STATUS "Version from git tag: ${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
        else()
            message(WARNING "Git tag '${GIT_TAG}' does not follow semantic versioning format")
            set(GIT_VERSION_MAJOR 0 PARENT_SCOPE)
            set(GIT_VERSION_MINOR 0 PARENT_SCOPE)
            set(GIT_VERSION_PATCH 0 PARENT_SCOPE)
            set(GIT_VERSION_FULL "0.0.0" PARENT_SCOPE)
        endif()
    else()
        # Fallback if no git or no tags found
        message(WARNING "Could not retrieve version from git tags, using fallback version 0.0.0")
        set(GIT_VERSION_MAJOR 0 PARENT_SCOPE)
        set(GIT_VERSION_MINOR 0 PARENT_SCOPE)
        set(GIT_VERSION_PATCH 0 PARENT_SCOPE)
        set(GIT_VERSION_FULL "0.0.0" PARENT_SCOPE)
    endif()
endfunction()
