cmake_minimum_required(VERSION 3.26)
project(sockets_test)

set(CMAKE_CXX_STANDARD 23)

include(${PROJECT_SOURCE_DIR}/project_options.cmake)

add_subdirectory(src)

if (${build_examples})
    add_subdirectory(examples)
endif ()
