add_executable(client client.cpp)
add_executable(server server.cpp)

target_link_libraries(client PRIVATE sockets project_options)
target_link_libraries(server PRIVATE sockets project_options)
