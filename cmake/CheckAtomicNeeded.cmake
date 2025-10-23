 # Check if libatomic is needed for std::atomic operations and link if necessary

 include(CheckCXXSourceCompiles)

function(check_and_link_atomic target_name)
    # First check if atomics work without any library
    check_cxx_source_compiles("
        #include <atomic>
        int main() {
            std::atomic<int> x;
            x.exchange(1);
            return 0;
        }
    " HAVE_CXX_ATOMICS_WITHOUT_LIB)

    if(NOT HAVE_CXX_ATOMICS_WITHOUT_LIB)
        # Try with -latomic
        set(CMAKE_REQUIRED_LIBRARIES atomic)
        check_cxx_source_compiles("
            #include <atomic>
            int main() {
                std::atomic<int> x;
                x.exchange(1);
                return 0;
            }
        " HAVE_CXX_ATOMICS_WITH_LIB)
        unset(CMAKE_REQUIRED_LIBRARIES)

        if(HAVE_CXX_ATOMICS_WITH_LIB)
            message(STATUS "Linking ${target_name} with libatomic")
            target_link_libraries(${target_name} PUBLIC atomic)
        else()
            message(WARNING "Atomic operations may not work correctly for ${target_name}")
        endif()
    else()
        message(STATUS "Atomic operations work without libatomic for ${target_name}")
    endif()
endfunction()
