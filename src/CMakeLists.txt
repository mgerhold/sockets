add_subdirectory(net)

add_executable(client
        client.cpp
)

target_link_libraries(client PRIVATE net)

add_executable(server
        server.cpp
)

target_link_libraries(server PRIVATE net)
