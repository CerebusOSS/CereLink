# these are cache variables, so they could be overwritten with -D,
set(CPACK_PACKAGE_NAME ${PROJECT_NAME} CACHE STRING "The resulting package name")
# which is useful in case of packing only selected components instead of the whole thing
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}"  CACHE STRING "Package description for the package metadata")
set(CPACK_PACKAGE_VENDOR "Blackrock Neurotech")
set(CPACK_PACKAGE_CONTACT "chadwick.boulay@gmail.com")
set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
#SET(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_SOURCE_DIR}/dist")
#set(CPACK_STRIP_FILES ON)
#set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
# set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

if(APPLE)
    set(CB_CPACK_DEFAULT_GEN TBZ2)  # DRAGNDROP?
    # set(CPACK_BUNDLE_ICON
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CB_ARCHIVE_ARCHITECTURE}")
elseif(WIN32)
    set(CB_CPACK_DEFAULT_GEN ZIP)
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_WIX_CMAKE_PACKAGE_REGISTRY ON)
    #    set(CPACK_WIX_UPGRADE_GUID "ee28a351-3b27-4c2b-8b48-259c87d1b1b4")
    #    set(CPACK_WIX_PROPERTY_ARPHELPLINK
    #            "https://labstreaminglayer.readthedocs.io/info/getting_started.html#getting-help")
elseif(UNIX)
    set(CB_CPACK_DEFAULT_GEN DEB)
    # https://unix.stackexchange.com/a/11552/254512
    set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/${PROJECT_NAME}")
#    set(CPACK_INSTALL_PREFIX "/usr")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Chadwick Boulay")
    # without this you won't be able to pack only specified component
    set(CPACK_DEBIAN_ENABLE_COMPONENT_DEPENDS ON)
    set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS ON)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
#    set(CPACK_DEB_COMPONENT_INSTALL ON)
#    set(CPACK_DEBIAN_PACKAGE_PRIORITY optional)
    # package name for deb. If set, then instead of some-application-0.9.2-Linux.deb
    # you'll get some-application_0.9.2_amd64.deb (note the underscores too)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
endif()
set(CPACK_GENERATOR ${CB_CPACK_DEFAULT_GEN} CACHE STRING "CPack pkg type(s) to generate")

# that is if you want every group to have its own package,
# although the same will happen if this is not set (so it defaults to ONE_PER_GROUP)
# and CPACK_DEB_COMPONENT_INSTALL is set to YES
#set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)#ONE_PER_GROUP)

# Directories to look for dependencies
set(DIRS ${CMAKE_BINARY_DIR})
include(CPack)
