# validate_clock_sync - PTP-based validation of ClockSync accuracy
# Linux-only: requires PTP hardware clock (PHC) access via clock_gettime()

if(UNIX AND NOT APPLE)
    add_executable(validate_clock_sync validate_clock_sync.cpp)
    target_link_libraries(validate_clock_sync PRIVATE cbsdk cbdev cbproto)
    target_include_directories(validate_clock_sync PRIVATE
        ${PROJECT_SOURCE_DIR}/src/cbsdk/include
        ${PROJECT_SOURCE_DIR}/src/cbdev/include
        ${PROJECT_SOURCE_DIR}/src/cbproto/include)
endif()
