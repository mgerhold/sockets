add_library(sockets
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

target_include_directories(sockets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (WIN32)
    set(SOCKET_LIBRARIES wsock32 ws2_32)
endif ()

target_link_libraries(sockets PRIVATE ${SOCKET_LIBRARIES} project_options)
