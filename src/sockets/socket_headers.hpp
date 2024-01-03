#pragma once

#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

namespace c2k {

    inline constexpr auto invalid_socket = std::uintptr_t{ INVALID_SOCKET };
    inline constexpr auto socket_error = SOCKET_ERROR;

    using SockLen = int;
    using SendReceiveSize = int;

} // namespace c2k
#else

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace c2k {

    inline constexpr auto invalid_socket = -1;
    inline constexpr auto socket_error = -1;

    using SockLen = unsigned int;
    using SendReceiveSize = std::size_t;

} // namespace c2k

inline int closesocket(int const socket) {
    return close(socket);
}

#endif
