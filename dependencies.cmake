include("${PROJECT_SOURCE_DIR}/cmake/CPM.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/system_link.cmake")

function(c2k_sockets_setup_dependencies)
    if (${c2k_sockets_build_tests})
        CPMAddPackage(
                NAME GOOGLE_TEST
                GITHUB_REPOSITORY google/googletest
                VERSION 1.14.0
                OPTIONS
                "BUILD_GMOCK OFF"
                "INSTALL_GTEST OFF"
        )
    endif ()
endfunction()
