#pragma once

#include "address_family.hpp"
#include "address_info.hpp"
#include "message_buffer.hpp"
#include "non_null_owner.hpp"
#include "synchronized.hpp"
#include "unique_value.hpp"
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace c2k {
    class TimeoutError final : public std::runtime_error {
    public:
        TimeoutError() : std::runtime_error{ "operation timed out" } { }
    };

    class ClientSocket;

    class AbstractSocket {
    public:
#ifdef _WIN32
        using OsSocketHandle = std::uintptr_t;
#else
        using OsSocketHandle = int;
#endif

    private:
        AddressInfo m_local_address_info;
        AddressInfo m_remote_address_info;

    protected:
        UniqueValue<OsSocketHandle, void (*)(OsSocketHandle handle)> m_socket_descriptor;

        explicit AbstractSocket(OsSocketHandle os_socket_handle);

    public:
        [[nodiscard]] std::optional<OsSocketHandle> os_socket_handle() const {
            if (not m_socket_descriptor.has_value()) {
                return std::nullopt;
            }
            return m_socket_descriptor.value();
        }

        AddressInfo const& local_address() && = delete;

        [[nodiscard]] AddressInfo const& local_address() const& {
            return m_local_address_info;
        }

        AddressInfo const& remote_address() && = delete;

        [[nodiscard]] AddressInfo const& remote_address() const& {
            return m_remote_address_info;
        }
    };

    namespace detail {
        [[nodiscard]] AbstractSocket::OsSocketHandle
        initialize_server_socket(AddressFamily address_family, std::uint16_t port);
    }

    class ServerSocket final : public AbstractSocket {
        friend class Sockets;

    private:
        std::jthread m_listen_thread;

        ServerSocket(AddressFamily address_family, std::uint16_t port, std::function<void(ClientSocket)> on_connect);

    public:
        ServerSocket(ServerSocket&& other) noexcept = default;
        ServerSocket& operator=(ServerSocket&& other) noexcept = default;

        ~ServerSocket();
        void stop();
    };

    class ClientSocket final : public AbstractSocket {
        friend class Sockets;
        friend void server_listen(
                std::stop_token const& stop_token,
                OsSocketHandle listen_socket,
                std::function<void(ClientSocket)> const& on_connect
        );

    private:
        struct SendTask {
            std::promise<std::size_t> promise;
            std::vector<std::byte> data;
        };

        struct ReceiveTask {
            std::promise<std::vector<std::byte>> promise;
            std::size_t max_num_bytes;
        };

        class State {
        private:
            NonNullOwner<std::atomic_bool> running{ make_non_null_owner<std::atomic_bool>(true) };

        public:
            Synchronized<std::deque<SendTask>> send_tasks{ std::deque<SendTask>{} };
            Synchronized<std::deque<ReceiveTask>> receive_tasks{ std::deque<ReceiveTask>{} };
            std::condition_variable_any data_received_condition_variable;
            std::condition_variable_any data_sent_condition_variable;

            [[nodiscard]] bool is_running() const {
                return *running;
            }

            void stop_running() {
                *running = false;
                data_received_condition_variable.notify_one();
                data_sent_condition_variable.notify_one();
            }

            void clear_queues();
        };

        std::unique_ptr<State> m_shared_state{ std::make_unique<State>() };
        std::jthread m_send_thread;
        std::jthread m_receive_thread;

        explicit ClientSocket(OsSocketHandle os_socket_handle);
        ClientSocket(AddressFamily address_family, std::string const& host, std::uint16_t port);

        static void keep_sending(State& state, OsSocketHandle socket);
        static void keep_receiving(State& state, OsSocketHandle socket);

        using Timeout = std::chrono::steady_clock::duration;

    public:
        ClientSocket(ClientSocket&& other) noexcept = default;
        ClientSocket& operator=(ClientSocket&& other) noexcept = default;

        ~ClientSocket();

        [[nodiscard]] bool is_connected() const {
            return m_shared_state->is_running();
        }

        // clang-format off
        [[nodiscard("discarding the return value may lead to the data to never be transmitted")]]
        std::future<std::size_t> send(std::vector<std::byte> data);

        [[nodiscard("discarding the return value may lead to the data to never be transmitted")]]
        std::future<std::size_t> send(std::string_view text);

        [[nodiscard("discarding the return value may lead to the data to never be transmitted")]]
        std::future<std::size_t> send(std::integral auto... values) {
            auto package = MessageBuffer{};
            (package << ... << values);
            return send(package);
        }

        [[nodiscard("discarding the return value may lead to the data to never be transmitted")]]
        std::future<std::size_t> send(MessageBuffer const& package) {
            return send(package.data);
        }
        // clang-format on

        [[nodiscard]] std::future<std::vector<std::byte>> receive(std::size_t max_num_bytes);
        [[nodiscard]] std::future<std::vector<std::byte>> receive(std::size_t max_num_bytes, Timeout timeout);
        [[nodiscard]] std::future<std::vector<std::byte>> receive_exact(std::size_t num_bytes);
        [[nodiscard]] std::future<std::vector<std::byte>> receive_exact(std::size_t num_bytes, Timeout timeout);
        [[nodiscard]] std::future<std::string> receive_string(std::size_t max_num_bytes);

        void close();

    private:
        [[nodiscard]] static bool process_receive_task(OsSocketHandle socket, ReceiveTask task);
        [[nodiscard]] static bool process_send_task(OsSocketHandle socket, SendTask task);
    };
} // namespace c2k
