#include "net/socket.hpp"
#include "socket_headers.hpp"
#include <cassert>
#include <format>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

[[nodiscard]] static constexpr int to_ai_family(AddressFamily const family) {
    switch (family) {
        case AddressFamily::Unspecified:
            return AF_UNSPEC;
        case AddressFamily::Ipv4:
            return AF_INET;
        case AddressFamily::Ipv6:
            return AF_INET6;
    }
    std::unreachable();
}

[[nodiscard]] static constexpr addrinfo generate_hints(AddressFamily const address_family, bool const is_passive) {
    auto hints = addrinfo{};
    hints.ai_family = to_ai_family(address_family);
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (is_passive) {
        hints.ai_flags = AI_PASSIVE;
    }
    return hints;
}

using AddressInfos = std::unique_ptr<addrinfo, decltype([](addrinfo* const pointer) { freeaddrinfo(pointer); })>;

// clang-format off
[[nodiscard]] static AddressInfos get_address_infos(
    AddressFamily const address_family,
    std::uint16_t const port,
    char const* const host = nullptr
) { // clang-format on
    auto const is_server = (host == nullptr);

    auto const hints = generate_hints(address_family, is_server);

    auto result = static_cast<addrinfo*>(nullptr);
    if (getaddrinfo(host, std::to_string(port).c_str(), &hints, &result) != 0) {
        throw std::runtime_error{ "unable to call getaddrinfo" };
    }
    if (result == nullptr) {
        throw std::runtime_error{ "no addresses found" };
    }
    return AddressInfos{ result };
}

[[nodiscard]] static Socket::OsSocketHandle create_socket(AddressInfos const& address_infos) {
    auto const socket = ::socket(address_infos->ai_family, address_infos->ai_socktype, address_infos->ai_protocol);
    if (socket == INVALID_SOCKET) {
        throw std::runtime_error{ std::format("failed to create socket: {}", WSAGetLastError()) };
    }
    return socket;
}

static void bind_socket(Socket::OsSocketHandle const socket, AddressInfos const& address_infos) {
    if (bind(socket, address_infos->ai_addr, static_cast<int>(address_infos->ai_addrlen)) == SOCKET_ERROR) {
        closesocket(socket);
        throw std::runtime_error{ "failed to bind socket" };
    }
}

static void connect_socket(Socket::OsSocketHandle const socket, AddressInfos const& address_infos) {
    if (connect(socket, address_infos->ai_addr, static_cast<int>(address_infos->ai_addrlen)) == SOCKET_ERROR) {
        closesocket(socket);
        throw std::runtime_error{ "unable to connect" };
    }
}

static void socket_deleter(Socket::OsSocketHandle const handle) {
    closesocket(handle);
}

Socket::Socket(OsSocketHandle const os_socket_handle) : m_socket_descriptor{ os_socket_handle, socket_deleter } { }

Socket::~Socket() {
    *m_running = false;
}

[[nodiscard]] std::future<std::size_t> Socket::send(std::vector<std::byte> data) {
    auto promise = std::promise<std::size_t>{};
    auto future = promise.get_future();

    return future;
}

//[[nodiscard]] std::future<std::vector<std::byte>> Socket::receive(std::size_t const max_num_bytes) { }

namespace detail {
    // clang-format off
    [[nodiscard]] Socket::OsSocketHandle initialize_server_socket(AddressFamily const address_family, std::uint16_t const port) {
        auto const address_infos = get_address_infos(address_family, port);
        auto const socket = create_socket(address_infos);
        bind_socket(socket, address_infos);
        return socket;
    }
    // clang-format on
} // namespace detail

[[nodiscard]] static auto
initialize_client_socket(AddressFamily const address_family, std::string const& host, std::uint16_t const port) {
    auto const address_infos = get_address_infos(address_family, port, host.c_str());
    auto const socket = create_socket(address_infos);
    connect_socket(socket, address_infos);
    return socket;
}

static void server_listen(
        Socket::OsSocketHandle const listen_socket,
        std::atomic_bool const& running,
        std::function<void(Socket)> const& on_connect
) {
    while (running) {
        auto file_descriptors_to_check = fd_set{};
        FD_ZERO(&file_descriptors_to_check);
        FD_SET(listen_socket, &file_descriptors_to_check);

        static constexpr auto timeout = timeval{ .tv_sec{ 0 }, .tv_usec{ 1000 * 100 } };
        auto const can_accept =
                static_cast<bool>(select(0 /* unused */, &file_descriptors_to_check, nullptr, nullptr, &timeout));
        if (not can_accept) {
            continue;
        }

        auto const client_socket = accept(listen_socket, nullptr, nullptr);
        assert(client_socket != INVALID_SOCKET and "successful acceptance is guaranteed by previous call to select");
        on_connect(Socket{ client_socket });
    }
}

ServerSocket::ServerSocket(
        AddressFamily const address_family,
        std::uint16_t const port,
        std::function<void(Socket)> on_connect
)
    : Socket{ detail::initialize_server_socket(address_family, port) } {
    assert(m_socket_descriptor.has_value() and "has been set via parent constructor");
    if (listen(m_socket_descriptor.value(), SOMAXCONN) == SOCKET_ERROR) {
        throw std::runtime_error{ std::format("failed to listen on socket: {}", WSAGetLastError()) };
    }

    m_listen_thread = std::jthread{ [this, callback = std::move(on_connect)] {
        server_listen(m_socket_descriptor.value(), *m_running, callback);
    } };
}

ServerSocket::~ServerSocket() {
    stop();
}

void ServerSocket::stop() {
    *m_running = false;
}

ClientSocket::ClientSocket(AddressFamily const address_family, std::string const& host, std::uint16_t const port)
    : Socket{ initialize_client_socket(address_family, host, port) } { }
