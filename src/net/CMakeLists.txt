add_library(net
        STATIC
        include/net/socket_lib.hpp
        socket_lib.cpp
        include/net/sandbox_client.hpp
        sandbox_client.cpp
        include/net/sandbox_server.hpp
        sandbox_server.cpp
        socket_headers.hpp
        include/net/socket.hpp
        include/net/address_family.hpp
        socket.cpp
        include/net/unique_value.hpp
        include/net/non_null_owner.hpp
)

target_include_directories(net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(net PRIVATE wsock32 ws2_32)
