#include "socket_headers.hpp"
#include "sockets/sockets.hpp"
#include <iostream>

namespace c2k {
    Sockets::Sockets() {
#ifdef _WIN32
        auto wsa_data = WSADATA{};
        static constexpr auto winsock_version = MAKEWORD(2, 2);
        if (WSAStartup(winsock_version, &wsa_data) != 0) {
            throw std::runtime_error{ "unable to initialize winsock" };
        }
#endif
    }

    Sockets::~Sockets() {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    // clang-format off
    [[nodiscard]] ClientSocket Sockets::create_client(
        AddressFamily const address_family,
        std::string const& host,
        std::uint16_t const port,
        Sockets const&
    ) { // clang-format on
        return ClientSocket{ address_family, host, port };
    }

    [[nodiscard]] Sockets const& Sockets::instance() {
        static auto handle = Sockets{};
        return handle;
    }
} // namespace c2k
