add_executable(c2k_sockets_tests synchronized_tests.cpp
        channel_tests.cpp)

target_link_libraries(c2k_sockets_tests PRIVATE c2k_sockets c2k_sockets_options)
target_link_system_libraries(c2k_sockets_tests PRIVATE GTest::gtest GTest::gtest_main)

add_test(NAME tests COMMAND c2k_sockets_tests)
