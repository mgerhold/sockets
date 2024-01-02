add_library(c2k_sockets
        STATIC
        include/sockets/socket_lib.hpp
        socket_lib.cpp
        socket_headers.hpp
        include/sockets/socket.hpp
        include/sockets/address_family.hpp
        socket.cpp
        include/sockets/unique_value.hpp
        include/sockets/non_null_owner.hpp
        include/sockets/synchronized.hpp
)

target_include_directories(c2k_sockets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (WIN32)
    set(SOCKET_LIBRARIES wsock32 ws2_32)
endif ()

target_link_libraries(c2k_sockets PRIVATE ${SOCKET_LIBRARIES} c2k_sockets_options)
