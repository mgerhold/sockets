#include "net/socket.hpp"
#include "socket_headers.hpp"
#include <cassert>
//#include <format>
#include <iostream>
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

[[nodiscard]] static AbstractSocket::OsSocketHandle create_socket(AddressInfos const& address_infos) {
    auto const socket = ::socket(address_infos->ai_family, address_infos->ai_socktype, address_infos->ai_protocol);
    if (socket == INVALID_SOCKET) {
        throw std::runtime_error{ "failed to create socket" };
    }
    return socket;
}

static void bind_socket(AbstractSocket::OsSocketHandle const socket, AddressInfos const& address_infos) {
    if (bind(socket, address_infos->ai_addr, static_cast<int>(address_infos->ai_addrlen)) == SOCKET_ERROR) {
        closesocket(socket);
        throw std::runtime_error{ "failed to bind socket" };
    }
}

static void connect_socket(AbstractSocket::OsSocketHandle const socket, AddressInfos const& address_infos) {
    if (connect(socket, address_infos->ai_addr, static_cast<int>(address_infos->ai_addrlen)) == SOCKET_ERROR) {
        closesocket(socket);
        throw std::runtime_error{ "unable to connect" };
    }
}

static void socket_deleter(AbstractSocket::OsSocketHandle const handle) {
    closesocket(handle);
    std::cerr << "socket closed\n";
}

AbstractSocket::AbstractSocket(OsSocketHandle const os_socket_handle)
    : m_socket_descriptor{ os_socket_handle, socket_deleter } { }


namespace detail {
    // clang-format off
    [[nodiscard]] AbstractSocket::OsSocketHandle initialize_server_socket(AddressFamily const address_family, std::uint16_t const port) {
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

void server_listen(
        AbstractSocket::OsSocketHandle const listen_socket,
        std::atomic_bool const& running,
        std::function<void(ClientSocket)> on_connect
) {
    while (running) {
        auto file_descriptors_to_check = fd_set{};
        FD_ZERO(&file_descriptors_to_check);
        FD_SET(listen_socket, &file_descriptors_to_check);

        static constexpr auto timeout = timeval{ .tv_sec{ 0 }, .tv_usec{ 1000 * 100 } };
        // clang-format off
        auto const select_result = select(
            static_cast<int>(listen_socket + 1), // ignored in Windows
            &file_descriptors_to_check,
            nullptr,
            nullptr,
            &timeout
        );
        // clang-format on
        if (select_result == SOCKET_ERROR) {
            throw std::runtime_error{ "failed to call select" };
        }
        auto const can_accept = (select_result == 1);
        if (not can_accept) {
            continue;
        }

        auto const client_socket = accept(listen_socket, nullptr, nullptr);
        assert(client_socket != INVALID_SOCKET and "successful acceptance is guaranteed by previous call to select");
        on_connect(ClientSocket{ client_socket });
    }
}

ServerSocket::ServerSocket(
        AddressFamily const address_family,
        std::uint16_t const port,
        std::function<void(ClientSocket)> on_connect
)
    : AbstractSocket{ detail::initialize_server_socket(address_family, port) } {
    assert(m_socket_descriptor.has_value() and "has been set via parent constructor");
    if (listen(m_socket_descriptor.value(), SOMAXCONN) == SOCKET_ERROR) {
        throw std::runtime_error{ "failed to listen on socket" };
    }

    m_listen_thread = std::jthread{ [this, callback = std::move(on_connect)]() mutable {
        server_listen(m_socket_descriptor.value(), *m_running, std::move(callback));
    } };
}

ServerSocket::~ServerSocket() {
    stop();
}

void ServerSocket::stop() {
    *m_running = false;
}

ClientSocket::ClientSocket(OsSocketHandle const os_socket_handle)
    : AbstractSocket{ os_socket_handle },
      m_send_receive_thread{ [&state = *m_shared_state, socket = m_socket_descriptor.value()] {
          keep_sending_and_receiving(state, socket);
      } } { }

ClientSocket::ClientSocket(AddressFamily const address_family, std::string const& host, std::uint16_t const port)
    : AbstractSocket{ initialize_client_socket(address_family, host, port) },
      m_send_receive_thread{ [&state = *m_shared_state, socket = m_socket_descriptor.value()] {
          keep_sending_and_receiving(state, socket);
      } } { }

void ClientSocket::keep_sending_and_receiving(State& state, OsSocketHandle const socket) {
    while (*state.running) {
        auto write_descriptors = fd_set{};
        FD_ZERO(&write_descriptors);
        FD_SET(socket, &write_descriptors);

        auto const timeout = timeval{ .tv_sec{ 0 }, .tv_usec{ 100 * 1000 } };
        // clang-format off
        auto const select_result = select(
            static_cast<int>(socket + 1),
            nullptr,
            &write_descriptors,
            nullptr,
            &timeout
        );
        // clang-format on

        if (select_result == SOCKET_ERROR) {
            throw std::runtime_error{ "failed to call select to check if socket is ready to send" };
        }

        auto const can_send = (select_result == 1);
        // std::cerr << "socket " << socket << " can send? " << std::boolalpha << can_send << '\n';

        if (can_send) {
            auto send_task = [&]() -> std::optional<SendTask> {
                auto const guard = std::scoped_lock{ state.send_tasks_mutex };
                if (state.send_tasks.empty()) {
                    return std::nullopt;
                }
                auto result = std::move(state.send_tasks.front());
                state.send_tasks.pop_front();
                return result;
            }();

            if (send_task.has_value()) {
                std::cerr << "  there is something to send...\n";
                if (not std::in_range<int>(send_task.value().data.size())) {
                    throw std::runtime_error{ "size of message to be sent exceeds allowed maximum" };
                }
                auto num_bytes_sent = std::size_t{ 0 };
                auto send_pointer = reinterpret_cast<char const*>(send_task.value().data.data());
                while (num_bytes_sent < send_task.value().data.size()) {
                    auto const num_bytes_remaining = send_task.value().data.size() - num_bytes_sent;
                    auto const result = ::send(socket, send_pointer, static_cast<int>(num_bytes_remaining), 0);
                    if (result == SOCKET_ERROR) {
                        throw std::runtime_error{ "error sending message" };
                    }
                    std::cerr << "  (part of) data sent! (" << result << " bytes)\n";
                    send_pointer += result;
                    num_bytes_sent += static_cast<std::size_t>(result);
                }
                std::cerr << "  data sent completely\n";
                send_task.value().promise.set_value(num_bytes_sent);
            }
        }

        // reading
        auto read_descriptors = fd_set{};
        FD_ZERO(&read_descriptors);
        FD_SET(socket, &read_descriptors);

        // clang-format off
        auto const read_select_result = select(
            static_cast<int>(socket + 1),
            &read_descriptors,
            nullptr,
            nullptr,
            &timeout
        );
        // clang-format on

        if (read_select_result == SOCKET_ERROR) {
            throw std::runtime_error{ "failed to call select to check if socket is ready to receive" };
        }

        auto const can_receive = (read_select_result == 1);

        if (can_receive) {
            auto receive_task = [&]() -> std::optional<ReceiveTask> {
                auto const guard = std::scoped_lock{ state.receive_tasks_mutex };
                if (state.receive_tasks.empty()) {
                    return std::nullopt;
                }
                auto result = std::move(state.receive_tasks.front());
                state.receive_tasks.pop_front();
                return result;
            }();

            if (receive_task.has_value()) {
                std::cerr << "  there is something to receive...\n";
                if (not std::in_range<int>(receive_task.value().max_num_bytes)) {
                    throw std::runtime_error{ "size of message to be received exceeds allowed maximum" };
                }

                auto receive_buffer = std::vector<std::byte>{};
                receive_buffer.resize(receive_task.value().max_num_bytes);

                auto const receive_result =
                        recv(socket,
                             reinterpret_cast<char*>(receive_buffer.data()),
                             static_cast<int>(receive_buffer.size()),
                             0);

                if (receive_result == 0) {
                    // connection has been gracefully closed => close socket
                    receive_task.value().promise.set_value({});
                    *state.running = false;
                    break;
                }

                if (receive_result == SOCKET_ERROR) {
                    throw std::runtime_error{ "failed to read from socket" };
                }

                std::cerr << "received " << receive_result << " bytes of a maximum of "
                          << receive_task.value().max_num_bytes << '\n';

                receive_buffer.resize(static_cast<std::size_t>(receive_result));

                receive_task.value().promise.set_value(std::move(receive_buffer));
            }
        }
    }
}

ClientSocket::~ClientSocket() {
    *m_shared_state->running = false;
}

// clang-format off
[[nodiscard("discarding the return value may lead to the data to never be transmitted")]]
std::future<std::size_t> ClientSocket::send(std::vector<std::byte> data) {
    auto promise = std::promise<std::size_t>{};
    auto future = promise.get_future();
    {
        auto const guard = std::scoped_lock{ m_shared_state->send_tasks_mutex };
        m_shared_state->send_tasks.emplace_back(std::move(promise), std::move(data));
    }
    return future;
}
// clang-format on

[[nodiscard]] std::future<std::vector<std::byte>> ClientSocket::receive(std::size_t const max_num_bytes) {
    auto promise = std::promise<std::vector<std::byte>>{};
    auto future = promise.get_future();
    {
        auto const guard = std::scoped_lock{ m_shared_state->receive_tasks_mutex };
        m_shared_state->receive_tasks.emplace_back(std::move(promise), max_num_bytes);
    }
    return future;
}
