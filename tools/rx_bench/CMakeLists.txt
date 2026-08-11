# rx_bench - STANDALONE UDP reception benchmark (packet-loss attribution)
add_executable(rx_bench rx_bench.cpp)
target_link_libraries(rx_bench PRIVATE cbsdk cbdev cbproto)
target_include_directories(rx_bench PRIVATE
    ${PROJECT_SOURCE_DIR}/src/cbsdk/include
    ${PROJECT_SOURCE_DIR}/src/cbdev/include
    ${PROJECT_SOURCE_DIR}/src/cbproto/include
    ${PROJECT_SOURCE_DIR}/src/cbutil/include)
