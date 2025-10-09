SET(SAMPLE_TARGET_LIST)
# simple_cbsdk is a bit weird because it does not link the library, but adds the library sources.
#  This is convenient for development, but also annoying because we have to manually add transitive dependencies.
#  TODO: It would be better if we made a simple obj target then reused that.
ADD_EXECUTABLE(simple_cbsdk
        ${PROJECT_SOURCE_DIR}/samples/SimpleCBSDK/simple_cbsdk.cpp
        ${LIB_SOURCE} ${LIB_HEADERS_MOC}
)
TARGET_INCLUDE_DIRECTORIES(simple_cbsdk PRIVATE
        ${LIB_INCL_DIRS}
        ${PROJECT_SOURCE_DIR}/cbsdk
)
TARGET_LINK_LIBRARIES(simple_cbsdk PRIVATE ${QT_LIBRARIES} CCFUtils)
IF(WIN32)
    TARGET_LINK_LIBRARIES(simple_cbsdk
            PRIVATE
            wsock32 ws2_32 winmm
    )
    target_include_directories(simple_cbsdk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/compat)
ELSEIF(NOT APPLE)
    # Hide unexported symbols
    TARGET_LINK_LIBRARIES(simple_cbsdk PRIVATE rt)
    SET_TARGET_PROPERTIES(simple_cbsdk PROPERTIES LINK_FLAGS "-Wl,--exclude-libs,ALL" )
ENDIF(WIN32)
LIST(APPEND SAMPLE_TARGET_LIST simple_cbsdk)

ADD_EXECUTABLE(simple_ccf
        ${PROJECT_SOURCE_DIR}/samples/SimpleCCF/simple_ccf.cpp
)
TARGET_LINK_LIBRARIES(simple_ccf ${LIB_NAME} )
LIST(APPEND SAMPLE_TARGET_LIST simple_ccf)

#    ADD_EXECUTABLE(test_parse_ccf
#            ${PROJECT_SOURCE_DIR}/samples/SimpleCCF/simple_parse_ccf.cpp
#    )
#    TARGET_LINK_LIBRARIES(simple_parse_ccf ${LIB_NAME} pugixml::pugixml)
#    LIST(APPEND SAMPLE_TARGET_LIST simple_parse_ccf)

ADD_EXECUTABLE(simple_io ${PROJECT_SOURCE_DIR}/samples/SimpleIO/simple_io.cpp)
TARGET_LINK_LIBRARIES(simple_io ${LIB_NAME} )
LIST(APPEND SAMPLE_TARGET_LIST simple_io)

ADD_EXECUTABLE(simple_callback ${PROJECT_SOURCE_DIR}/samples/SimpleIO/simple_callback.cpp)
TARGET_LINK_LIBRARIES(simple_callback ${LIB_NAME} )
LIST(APPEND SAMPLE_TARGET_LIST simple_callback)

ADD_EXECUTABLE(simple_comments ${PROJECT_SOURCE_DIR}/samples/SimpleComments/simple_comments.cpp )
TARGET_LINK_LIBRARIES(simple_comments ${LIB_NAME} )
LIST(APPEND SAMPLE_TARGET_LIST simple_comments)

ADD_EXECUTABLE(simple_analog_out ${PROJECT_SOURCE_DIR}/samples/SimpleAnalogOut/simple_analog_out.cpp)
TARGET_LINK_LIBRARIES(simple_analog_out ${LIB_NAME})
LIST(APPEND SAMPLE_TARGET_LIST simple_analog_out)

LIST(APPEND INSTALL_TARGET_LIST ${SAMPLE_TARGET_LIST})

IF(WIN32)
    findQtInstallationTool("windeployqt")
    add_custom_command(TARGET testcbsdk POST_BUILD
            COMMAND ${QT_DEPLOYQT_EXECUTABLE} $<TARGET_FILE:${LIB_NAME}>)
    add_custom_command(TARGET test_io POST_BUILD
            COMMAND ${QT_DEPLOYQT_EXECUTABLE} $<TARGET_FILE:${LIB_NAME}>)
    add_custom_command(TARGET simple_callback POST_BUILD
            COMMAND ${QT_DEPLOYQT_EXECUTABLE} $<TARGET_FILE:${LIB_NAME}>)
    add_custom_command(TARGET test_comments POST_BUILD
            COMMAND ${QT_DEPLOYQT_EXECUTABLE} $<TARGET_FILE:${LIB_NAME}>)
    add_custom_command(TARGET test_analog_out POST_BUILD
            COMMAND ${QT_DEPLOYQT_EXECUTABLE} $<TARGET_FILE:${LIB_NAME}>)
ENDIF(WIN32)

if(NOT CMAKE_INSTALL_RPATH)
    set(LIBDIR "../lib64")
    foreach(test_app ${TEST_SAMPLE_LIST})
        if(APPLE)
            set_property(TARGET ${test_app} APPEND
                    PROPERTY INSTALL_RPATH "@executable_path/;@executable_path/${LIBDIR};@executable_path/../Frameworks")
        elseif(UNIX)
            set_property(TARGET ${test_app}
                    PROPERTY INSTALL_RPATH "\$ORIGIN:\$ORIGIN/${LIBDIR}")
        endif(APPLE)
    endforeach()
endif(NOT CMAKE_INSTALL_RPATH)