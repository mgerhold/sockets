add_library(c2k_sockets
        STATIC
        sockets.cpp
        socket_headers.hpp
        include/sockets/detail/socket.hpp
        include/sockets/detail/address_family.hpp
        socket.cpp
        include/sockets/sockets.hpp
        include/sockets/detail/address_info.hpp
        include/sockets/detail/byte_order.hpp
        include/sockets/detail/message_buffer.hpp
        include/sockets/detail/utils.hpp
)

target_include_directories(c2k_sockets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (WIN32)
    set(SOCKET_LIBRARIES wsock32 ws2_32)
endif ()

target_link_libraries(c2k_sockets PRIVATE ${SOCKET_LIBRARIES} c2k_sockets_options)

target_link_system_libraries(c2k_sockets PUBLIC lib2k)
