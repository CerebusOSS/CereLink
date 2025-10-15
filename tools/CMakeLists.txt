##
# n2h5
if(${CBSDK_BUILD_HDF5})
    find_package( HDF5 COMPONENTS C HL)
endif()
if(${CBSDK_BUILD_HDF5} AND HDF5_FOUND)
    set(N2H5_SOURCE
            ${PROJECT_SOURCE_DIR}/n2h5/main.cpp
            ${PROJECT_SOURCE_DIR}/n2h5/n2h5.cpp
            ${PROJECT_SOURCE_DIR}/n2h5/n2h5.h
            ${PROJECT_SOURCE_DIR}/n2h5/NevNsx.h
    )
    message(STATUS "Add n2h5 utility build target")
    if(MSVC)
        set(N2H5_SOURCE ${N2H5_SOURCE} ${PROJECT_SOURCE_DIR}/n2h5/res/n2h5_res.rc)
    endif(MSVC)
    add_executable( ${N2H5_NAME} ${N2H5_SOURCE} )
    target_include_directories( ${N2H5_NAME} PRIVATE ${LIB_INCL_DIRS} ${HDF5_INCLUDE_DIRS})
    target_link_libraries(${N2H5_NAME} ${HDF5_LIBRARIES} ${HDF5_HL_LIBRARIES})
    list(APPEND INSTALL_TARGET_LIST ${N2H5_NAME})
endif(${CBSDK_BUILD_HDF5} AND HDF5_FOUND )