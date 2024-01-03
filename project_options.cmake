include(${PROJECT_SOURCE_DIR}/cmake/warnings.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/sanitizers.cmake)

# the following function was taken from:
# https://github.com/cpp-best-practices/cmake_template/blob/main/ProjectOptions.cmake
macro(check_sanitizer_support)
    if ((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND NOT WIN32)
        set(supports_ubsan ON)
    else ()
        set(supports_ubsan OFF)
    endif ()

    if ((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND WIN32)
        set(supports_asan OFF)
    else ()
        set(supports_asan ON)
    endif ()
endmacro()

check_sanitizer_support()

if (PROJECT_IS_TOP_LEVEL)
    option(warnings_as_errors "Treat warnings as errors" ON)
    option(enable_undefined_behavior_sanitizer "Enable undefined behavior sanitizer" ${supports_ubsan})
    option(enable_address_sanitizer "Enable address sanitizer" ${supports_asan})
    option(enable_thread_sanitizer "Enable thread sanitizer" OFF)
    option(build_examples "Build example server and client applications" ON)
else ()
    option(warnings_as_errors "Treat warnings as errors" OFF)
    option(enable_undefined_behavior_sanitizer "Enable undefined behavior sanitizer" OFF)
    option(enable_address_sanitizer "Enable address sanitizer" OFF)
    option(enable_thread_sanitizer "Enable thread sanitizer" OFF)
    option(build_examples "Build example server and client applications" OFF)
endif ()

add_library(c2k_sockets_warnings INTERFACE)
set_warnings(c2k_sockets_warnings ${warnings_as_errors})

add_library(c2k_sockets_sanitizers INTERFACE)
enable_sanitizers(c2k_sockets_sanitizers ${enable_address_sanitizer} ${enable_undefined_behavior_sanitizer} ${enable_thread_sanitizer})

add_library(c2k_sockets_options INTERFACE)
target_link_libraries(c2k_sockets_options
        INTERFACE c2k_sockets_warnings
        INTERFACE c2k_sockets_sanitizers
)
