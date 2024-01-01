#pragma once

#include "address_family.hpp"
#include "non_null_owner.hpp"
#include "unique_value.hpp"

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <thread>

class Socket {
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

    struct SendTask {
        std::promise<std::size_t> promise;
        std::vector<std::byte> data;
    };

    struct ReceiveTask {
        std::promise<std::vector<std::byte>> promise;
        std::size_t max_num_bytes;
    };

protected:
    UniqueValue<OsSocketHandle, void (*)(OsSocketHandle handle)> m_socket_descriptor;
    NonNullOwner<std::atomic_bool> m_running{ make_non_null_owner<std::atomic_bool>(true) };

    explicit Socket(OsSocketHandle os_socket_handle);

public:
    ~Socket();

    [[nodiscard]] std::future<std::size_t> send(std::vector<std::byte> data);
    [[nodiscard]] std::future<std::vector<std::byte>> receive(std::size_t max_num_bytes);

    [[nodiscard]] std::optional<OsSocketHandle> os_socket_handle() const {
        if (not m_socket_descriptor.has_value()) {
            return std::nullopt;
        }
        return m_socket_descriptor.value();
    }
};

namespace detail {
    [[nodiscard]] Socket::OsSocketHandle initialize_server_socket(AddressFamily address_family, std::uint16_t port);
}

class ServerSocket final : public Socket {
    friend class SocketLib;

private:
    std::jthread m_listen_thread;

    ServerSocket(AddressFamily address_family, std::uint16_t port, std::function<void(Socket)> on_connect);

public:
    ~ServerSocket();

    void stop();
};

class ClientSocket final : public Socket {
    friend class SocketLib;

private:
    ClientSocket(AddressFamily address_family, std::string const& host, std::uint16_t port);
};
