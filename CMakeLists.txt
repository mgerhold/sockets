cmake_minimum_required(VERSION 3.26)
project(sockets)

set(CMAKE_CXX_STANDARD 23)

include(${PROJECT_SOURCE_DIR}/project_options.cmake)
include(${PROJECT_SOURCE_DIR}/dependencies.cmake)

c2k_sockets_setup_dependencies()

add_subdirectory(src)

if (${c2k_sockets_build_examples})
    add_subdirectory(examples)
endif ()

if (${c2k_sockets_build_tests})
    enable_testing()
    add_subdirectory(test)
endif()