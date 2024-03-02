#pragma once

#include <cstdlib>
#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

namespace c2k {

    inline constexpr auto invalid_socket = std::uintptr_t{ INVALID_SOCKET };
    inline constexpr auto socket_error = SOCKET_ERROR;
    inline constexpr auto reuse_port = SO_REUSEADDR;
    inline constexpr auto tcp_no_delay = TCP_NODELAY;
    inline constexpr auto send_flags = 0;

    using SockLen = int;
    using SendReceiveSize = int;

} // namespace c2k
#else

#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace c2k {

    inline constexpr auto invalid_socket = -1;
    inline constexpr auto socket_error = -1;
    inline constexpr auto reuse_port = SO_REUSEPORT;
    inline constexpr auto tcp_no_delay = TCP_NODELAY;
    inline constexpr auto send_flags = MSG_NOSIGNAL;

    using SockLen = unsigned int;
    using SendReceiveSize = std::size_t;

} // namespace c2k

inline int closesocket(int const socket) {
    return close(socket);
}

#endif
