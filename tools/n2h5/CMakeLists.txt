##
# n2h5
find_package( HDF5 COMPONENTS C HL)
if(HDF5_FOUND)
    set(N2H5_SOURCE
            ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/n2h5.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/n2h5.h
            ${CMAKE_CURRENT_SOURCE_DIR}/NevNsx.h
    )
    message(STATUS "Add n2h5 utility build target")
    if(MSVC)
        set(N2H5_SOURCE ${N2H5_SOURCE} ${CMAKE_CURRENT_SOURCE_DIR}/res/n2h5_res.rc)
    endif(MSVC)
    add_executable( n2h5 ${N2H5_SOURCE} )
    target_include_directories( n2h5 PRIVATE ${LIB_INCL_DIRS} ${HDF5_INCLUDE_DIRS})
    target_link_libraries(n2h5 ${HDF5_LIBRARIES} ${HDF5_HL_LIBRARIES})
    list(APPEND INSTALL_TARGET_LIST n2h5)
endif(HDF5_FOUND )
