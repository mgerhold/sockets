#include "socket_headers.hpp"
#include "sockets/sockets.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace c2k {
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

    enum class SelectStatusCategory {
        Read,
        Write,
        Except,
    };

    [[nodiscard]] static fd_set generate_fd_set(AbstractSocket::OsSocketHandle const socket) {
        auto descriptors = fd_set{};
        FD_ZERO(&descriptors);
        FD_SET(socket, &descriptors);
        return descriptors;
    }

    [[nodiscard]] static bool is_socket_ready(
            AbstractSocket::OsSocketHandle const socket,
            SelectStatusCategory const category,
            std::size_t const timeout_milliseconds
    ) {
        auto const microseconds = timeout_milliseconds * 1000;
        auto timeout = timeval{ .tv_sec = static_cast<long>(microseconds / (1000 * 1000)),
                                .tv_usec = static_cast<long>(microseconds % (1000 * 1000)) };
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
        if (select_result == socket_error) {
            throw std::runtime_error{ "failed to call select on socket" };
        }
        return select_result == 1;
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
        if (socket == invalid_socket) {
            throw std::runtime_error{ "failed to create socket" };
        }
#ifdef _WIN32
        auto flag = char{ 1 };
#else
        auto flag = 1;
#endif
        auto const result = ::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        if (result < 0) {
            throw std::runtime_error{ "failed to set TCP_NODELAY" };
        }
        return socket;
    }

    static void bind_socket(AbstractSocket::OsSocketHandle const socket, AddressInfos const& address_infos) {
        if (bind(socket, address_infos->ai_addr, static_cast<SockLen>(address_infos->ai_addrlen)) == socket_error) {
            closesocket(socket);
            throw std::runtime_error{ "failed to bind socket" };
        }
    }

    static void connect_socket(AbstractSocket::OsSocketHandle const socket, AddressInfos const& address_infos) {
        if (connect(socket, address_infos->ai_addr, static_cast<SockLen>(address_infos->ai_addrlen)) == socket_error) {
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
        [[nodiscard]] AbstractSocket::OsSocketHandle initialize_server_socket(
            AddressFamily const address_family,
            std::uint16_t const port
        ) {
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
            auto const can_accept = is_socket_ready(listen_socket, SelectStatusCategory::Read, 100);
            if (not can_accept) {
                continue;
            }

            auto const client_socket = accept(listen_socket, nullptr, nullptr);
            assert(client_socket != invalid_socket and "successful acceptance is guaranteed by previous call to select"
            );
#ifdef _WIN32
            auto flag = char{ 1 };
#else
            auto flag = 1;
#endif
            auto const result = ::setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
            if (result < 0) {
                throw std::runtime_error{ "failed to set TCP_NODELAY" };
            }
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
        if (listen(m_socket_descriptor.value(), SOMAXCONN) == socket_error) {
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
          m_send_thread{ keep_sending, std::ref(*m_shared_state), m_socket_descriptor.value() },
          m_receive_thread{ keep_receiving, std::ref(*m_shared_state), m_socket_descriptor.value() } { }

    ClientSocket::ClientSocket(AddressFamily const address_family, std::string const& host, std::uint16_t const port)
        : ClientSocket{ initialize_client_socket(address_family, host, port) } { }

    template<typename Queue, typename Element = typename Queue::value_type>
    [[nodiscard]] static std::optional<Element> try_dequeue_task(Synchronized<Queue>& queue) {
        auto tasks = queue.lock();
        if (tasks->empty()) {
            return std::nullopt;
        }
        auto result = std::move(tasks->front());
        tasks->pop_front();
        return result;
    }

    void ClientSocket::keep_sending(State& state, OsSocketHandle const socket) {
        while (*state.running) {
            auto processed_send_task = false;
            if (auto send_task = try_dequeue_task(state.send_tasks)) {
                processed_send_task = true;
                if (not process_send_task(socket, *std::move(send_task))) {
                    // connection is dead
                    *state.running = false;
                    break;
                }
            }

            if (not processed_send_task) {
                auto lock = std::unique_lock{ state.data_sent_mutex };
                state.data_sent_condition_variable.wait(lock);
            }
        }
    }

    void ClientSocket::keep_receiving(State& state, OsSocketHandle const socket) {
        while (*state.running) {
            auto processed_receive_task = false;
            if (auto receive_task = try_dequeue_task(state.receive_tasks)) {
                processed_receive_task = true;
                if (not process_receive_task(socket, *std::move(receive_task))) {
                    // connection is dead
                    *state.running = false;
                    break;
                }
            }

            if (not processed_receive_task) {
                auto lock = std::unique_lock{ state.data_received_mutex };
                state.data_received_condition_variable.wait(lock);
            }
        }
    }

    ClientSocket::~ClientSocket() {
        if (m_shared_state != nullptr) {
            // if this object was moved from, the cleanup will be done by the object
            // this object was moved into
            *(m_shared_state->running) = false;
            m_shared_state->data_sent_condition_variable.notify_one();
            m_shared_state->data_received_condition_variable.notify_one();
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
        m_shared_state->data_sent_condition_variable.notify_one();
        return future;
    }

    // clang-format off
    [[nodiscard("discarding the return value may lead to the data to never be transmitted")]]
    std::future<std::size_t> ClientSocket::send(std::string_view const text) {
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
        m_shared_state->data_received_condition_variable.notify_one();
        return future;
    }

    [[nodiscard]] std::future<std::string> ClientSocket::receive_string(std::size_t const max_num_bytes) {
        return std::async([this, max_num_bytes]() -> std::string {
            auto const data = receive(max_num_bytes).get();
            auto result = std::string{};
            result.resize(data.size());
            std::memcpy(result.data(), data.data(), data.size());
            return result;
        });
    }

    [[nodiscard]] bool ClientSocket::process_receive_task(OsSocketHandle const socket, ReceiveTask task) {
        if (not std::in_range<SendReceiveSize>(task.max_num_bytes)) {
            throw std::runtime_error{ "size of message to be received exceeds allowed maximum" };
        }

        auto receive_buffer = std::vector<std::byte>{};
        receive_buffer.resize(task.max_num_bytes);

        auto const receive_result =
                recv(socket,
                     reinterpret_cast<char*>(receive_buffer.data()),
                     static_cast<SendReceiveSize>(receive_buffer.size()),
                     0);

        if (receive_result == 0) {
            // connection has been gracefully closed => close socket
            task.promise.set_value({});
            return false;
        }

        if (receive_result == socket_error) {
            throw std::runtime_error{ "failed to read from socket" };
        }

        receive_buffer.resize(static_cast<std::size_t>(receive_result));

        task.promise.set_value(std::move(receive_buffer));
        return true;
    }

    [[nodiscard]] bool ClientSocket::process_send_task(OsSocketHandle const socket, SendTask task) {
        if (not std::in_range<SendReceiveSize>(task.data.size())) {
            throw std::runtime_error{ "size of message to be sent exceeds allowed maximum" };
        }
        auto num_bytes_sent = std::size_t{ 0 };
        auto send_pointer = reinterpret_cast<char const*>(task.data.data());
        while (num_bytes_sent < task.data.size()) {
            auto const num_bytes_remaining = task.data.size() - num_bytes_sent;
            auto const result = ::send(socket, send_pointer, static_cast<SendReceiveSize>(num_bytes_remaining), 0);
            if (result == socket_error) {
#ifdef _WIN32
                auto const error = WSAGetLastError();
                if (error == WSAENOTCONN or error == WSAECONNABORTED) {
#else
                if (errno == ENOTCONN or errno == ECONNRESET) {
#endif
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
} // namespace c2k
