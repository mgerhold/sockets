#include <iostream>
#include <net/socket_lib.hpp>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
// todo
#endif

SocketLib::SocketLib() {
    auto wsa_data = WSADATA{};
    static constexpr auto winsock_version = MAKEWORD(2, 2);
    if (WSAStartup(winsock_version, &wsa_data) != 0) {
        throw std::runtime_error{ "unable to initialize winsock" };
    }
}

SocketLib::~SocketLib() {
    WSACleanup();
}

// clang-format off
[[nodiscard]] ClientSocket SocketLib::create_client_socket(
    AddressFamily const address_family,
    std::string const& host,
    std::uint16_t const port,
    SocketLib const&
) { // clang-format on
    return ClientSocket{ address_family, host, port };
}

[[nodiscard]] SocketLib const& SocketLib::instance() {
    static auto handle = SocketLib{};
    return handle;
}
