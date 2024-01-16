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
        CPMAddPackage(
                NAME TL_EXPECTED
                GITHUB_REPOSITORY TartanLlama/expected
                VERSION 1.1.0
                OPTIONS
                "EXPECTED_BUILD_PACKAGE OFF"
                "EXPECTED_BUILD_TESTS OFF"
                "EXPECTED_BUILD_PACKAGE_DEB OFF"
        )
    endif ()
endfunction()
