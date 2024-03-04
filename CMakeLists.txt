cmake_minimum_required(VERSION 3.21.7)
project(sockets)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${PROJECT_SOURCE_DIR}/project_options.cmake)
include(${PROJECT_SOURCE_DIR}/dependencies.cmake)

c2k_sockets_setup_dependencies()

add_subdirectory(src bin)

#export(
#        TARGETS c2k_sockets c2k_sockets_options c2k_sockets_warnings c2k_sockets_sanitizers
#        NAMESPACE c2k::
#        FILE ${CMAKE_BINARY_DIR}/cmake/c2k-sockets-targets.cmake
#)

if (${c2k_sockets_build_examples})
    add_subdirectory(examples)
endif ()

if (${c2k_sockets_build_tests})
    enable_testing()
    add_subdirectory(test)
endif ()
