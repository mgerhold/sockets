cmake_minimum_required(VERSION 3.26)
project(sockets)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${PROJECT_SOURCE_DIR}/project_options.cmake)
include(${PROJECT_SOURCE_DIR}/dependencies.cmake)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

c2k_sockets_setup_dependencies()

add_subdirectory(src bin)


if (${c2k_sockets_build_examples})
    add_subdirectory(examples)
endif ()

if (${c2k_sockets_build_tests})
    enable_testing()
    add_subdirectory(test)
endif ()
