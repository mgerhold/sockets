#pragma once

#include "address_family.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <thread>

class Socket {
    friend class ServerSocket;
    friend class OsSocketHandleTypeExposer;

public:
#ifdef _WIN32
    using OsSocketHandle = std::uintptr_t;
#else
    using OsSocketHandle = int;
#endif

private:
    friend void server_listen(
            OsSocketHandle listen_socket,
            std::atomic_bool const& running,
            std::function<void(Socket)> const& on_connect
    );

protected:
    std::optional<OsSocketHandle> m_socket_descriptor;

    explicit Socket(OsSocketHandle os_socket_handle);

public:
    Socket(Socket const& other) = delete;
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket const& other) = delete;
    Socket& operator=(Socket&& other) noexcept;
    ~Socket();

    [[nodiscard]] std::optional<OsSocketHandle> os_socket_handle() const {
        return m_socket_descriptor;
    }
};

namespace detail {
    [[nodiscard]] Socket::OsSocketHandle initialize_server_socket(AddressFamily address_family, std::uint16_t port);
}

class ServerSocket final : public Socket {
    friend class SocketLib;

    std::jthread m_listen_thread;
    std::atomic_bool m_running{ true };

private:
    ServerSocket(AddressFamily address_family, std::uint16_t port, std::function<void(Socket)> on_connect);

public:
    ServerSocket(ServerSocket const& other) = delete;
    ServerSocket(ServerSocket&& other) noexcept = delete;
    ServerSocket& operator=(ServerSocket const& other) = delete;
    ServerSocket& operator=(ServerSocket&& other) noexcept = delete;
    ~ServerSocket();

    void stop();
};

class ClientSocket final : public Socket {
    friend class SocketLib;

private:
    ClientSocket(AddressFamily address_family, std::string const& host, std::uint16_t port);
};
