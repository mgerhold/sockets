#include "net/socket.hpp"
#include "socket_headers.hpp"
#include <cassert>
#include <cstring>
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
        std::stop_token const& stop_token,
        AbstractSocket::OsSocketHandle const listen_socket,
        std::function<void(ClientSocket)> const& on_connect
) {
    while (not stop_token.stop_requested()) {
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

    m_listen_thread = std::jthread{ server_listen, m_socket_descriptor.value(), std::move(on_connect) };
}

ServerSocket::~ServerSocket() {
    stop();
}

void ServerSocket::stop() {
    m_listen_thread.request_stop();
}

ClientSocket::ClientSocket(OsSocketHandle const os_socket_handle)
    : AbstractSocket{ os_socket_handle },
      m_send_receive_thread{ keep_sending_and_receiving, std::ref(*m_shared_state), m_socket_descriptor.value() } { }

ClientSocket::ClientSocket(AddressFamily const address_family, std::string const& host, std::uint16_t const port)
    : AbstractSocket{ initialize_client_socket(address_family, host, port) },
      m_send_receive_thread{ keep_sending_and_receiving, std::ref(*m_shared_state), m_socket_descriptor.value() } { }

[[nodiscard]] static fd_set generate_fd_set(AbstractSocket::OsSocketHandle const socket) {
    auto descriptors = fd_set{};
    FD_ZERO(&descriptors);
    FD_SET(socket, &descriptors);
    return descriptors;
}

enum class SelectStatusCategory {
    Read,
    Write,
    Except,
};

[[nodiscard]] bool is_socket_ready(
        AbstractSocket::OsSocketHandle const socket,
        SelectStatusCategory const category,
        std::size_t const timeout_milliseconds
) {
    auto const microseconds = timeout_milliseconds * 1000;
    auto const timeout = timeval{ .tv_sec{ static_cast<long>(microseconds / (1000 * 1000)) },
                                  .tv_usec{ static_cast<long>(microseconds % (1000 * 1000)) } };
    auto const select_result = [&] {
        auto descriptors = generate_fd_set(socket);
        switch (category) {
            case SelectStatusCategory::Read:
                return select(static_cast<int>(socket + 1), &descriptors, nullptr, nullptr, &timeout);
            case SelectStatusCategory::Write:
                return select(static_cast<int>(socket + 1), nullptr, &descriptors, nullptr, &timeout);
            case SelectStatusCategory::Except:
                return select(static_cast<int>(socket + 1), nullptr, nullptr, &descriptors, &timeout);
            default:
                std::unreachable();
                break;
        }
    }();
    if (select_result == SOCKET_ERROR) {
        throw std::runtime_error{ "failed to call select on socket" };
    }
    return select_result == 1;
}

template<typename Queue, typename Element = typename Queue::value_type>
[[nodiscard]] static std::optional<Element> try_enqueue_task(Synchronized<Queue>& queue) {
    auto tasks = queue.lock();
    if (tasks->empty()) {
        return std::nullopt;
    }
    auto result = std::move(tasks->front());
    tasks->pop_front();
    return result;
}

void ClientSocket::keep_sending_and_receiving(State& state, OsSocketHandle const socket) {
    static constexpr auto timeout_milliseconds = std::size_t{ 100 };
    while (*state.running) {
        if (is_socket_ready(socket, SelectStatusCategory::Write, timeout_milliseconds)) {
            if (auto send_task = try_enqueue_task(state.send_tasks)) {
                if (not process_send_task(socket, *std::move(send_task))) {
                    // connection is dead
                    *state.running = false;
                    break;
                }
            }
        }

        if (is_socket_ready(socket, SelectStatusCategory::Read, timeout_milliseconds)) {
            if (auto receive_task = try_enqueue_task(state.receive_tasks)) {
                if (not process_receive_task(socket, *std::move(receive_task))) {
                    // connection is dead
                    *state.running = false;
                    break;
                }
            }
        }
    }
}

ClientSocket::~ClientSocket() {
    if (m_shared_state != nullptr) {
        // if this object was moved from, the cleanup will be done by the object
        // this object was moved into
        *(m_shared_state->running) = false;
    }
}

// clang-format off
[[nodiscard("discarding the return value may lead to the data to never be transmitted")]]
std::future<std::size_t> ClientSocket::send(std::vector<std::byte> data) {
    // clang-format on
    auto promise = std::promise<std::size_t>{};
    auto future = promise.get_future();
    {
        auto send_tasks = m_shared_state->send_tasks.lock();
        send_tasks->emplace_back(std::move(promise), std::move(data));
    }
    return future;
}

// clang-format off
[[nodiscard("discarding the return value may lead to the data to never be transmitted")]]
std::future<std::size_t> ClientSocket::send(std::string_view const text){
    // clang-format on
    auto data = std::vector<std::byte>{};
    data.resize(text.length(), std::byte{});
    std::memcpy(data.data(), text.data(), text.size());
    return send(std::move(data));
}

[[nodiscard]] std::future<std::vector<std::byte>> ClientSocket::receive(std::size_t const max_num_bytes) {
    auto promise = std::promise<std::vector<std::byte>>{};
    auto future = promise.get_future();
    {
        auto receive_tasks = m_shared_state->receive_tasks.lock();
        receive_tasks->emplace_back(std::move(promise), max_num_bytes);
    }
    return future;
}

[[nodiscard]] std::future<std::string> ClientSocket::receive_string(std::size_t const max_num_bytes) {
    return std::async([&]() -> std::string {
        auto const data = receive(max_num_bytes).get();
        auto result = std::string{};
        result.resize(data.size());
        std::memcpy(result.data(), data.data(), data.size());
        return result;
    });
}

[[nodiscard]] bool ClientSocket::process_receive_task(OsSocketHandle const socket, ReceiveTask task) {
    if (not std::in_range<int>(task.max_num_bytes)) {
        throw std::runtime_error{ "size of message to be received exceeds allowed maximum" };
    }

    auto receive_buffer = std::vector<std::byte>{};
    receive_buffer.resize(task.max_num_bytes);

    auto const receive_result =
            recv(socket, reinterpret_cast<char*>(receive_buffer.data()), static_cast<int>(receive_buffer.size()), 0);

    if (receive_result == 0) {
        // connection has been gracefully closed => close socket
        task.promise.set_value({});
        return false;
    }

    if (receive_result == SOCKET_ERROR) {
        throw std::runtime_error{ "failed to read from socket" };
    }

    receive_buffer.resize(static_cast<std::size_t>(receive_result));

    task.promise.set_value(std::move(receive_buffer));
    return true;
}

[[nodiscard]] bool ClientSocket::process_send_task(OsSocketHandle const socket, SendTask task) {
    if (not std::in_range<int>(task.data.size())) {
        throw std::runtime_error{ "size of message to be sent exceeds allowed maximum" };
    }
    auto num_bytes_sent = std::size_t{ 0 };
    auto send_pointer = reinterpret_cast<char const*>(task.data.data());
    while (num_bytes_sent < task.data.size()) {
        auto const num_bytes_remaining = task.data.size() - num_bytes_sent;
        auto const result = ::send(socket, send_pointer, static_cast<int>(num_bytes_remaining), 0);
        if (result == SOCKET_ERROR) {
            auto const error = WSAGetLastError();
            if (error == WSAENOTCONN or error == WSAECONNABORTED) {
                // connection no longer active
                task.promise.set_value(0);
                return false;
            }
            throw std::runtime_error{ "error sending message" };
        }
        send_pointer += result;
        num_bytes_sent += static_cast<std::size_t>(result);
    }
    task.promise.set_value(num_bytes_sent);
    return true;
}
