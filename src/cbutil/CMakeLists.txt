add_library(cbutil INTERFACE)
target_include_directories(cbutil INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(cbutil INTERFACE cxx_std_17)

install(TARGETS cbutil
    EXPORT CBSDKTargets
)
