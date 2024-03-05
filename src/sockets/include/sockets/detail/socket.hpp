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

    class ReadError final : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
        ReadError() : std::runtime_error{ "error reading from socket" } { }
    };

    class SendError final : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    class ClientSocket;

    /**
     * @class AbstractSocket
     * @brief Represents an abstract socket for communication
     *
     * This class provides a common interface and functionality for socket operations. It manages the
     * socket descriptor, local and remote addresses.
     */
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
        /**
         * Returns the operating system socket handle associated with the AbstractSocket.
         *
         * If the socket descriptor has a value, it returns the value encapsulated in std::optional.
         * Otherwise, it returns std::nullopt.
         *
         * @return The operating system socket handle as std::optional<OsSocketHandle>.
         */
        [[nodiscard]] std::optional<OsSocketHandle> os_socket_handle() const {
            if (not m_socket_descriptor.has_value()) {
                return std::nullopt;
            }
            return m_socket_descriptor.value();
        }

        AddressInfo const& local_address() && = delete;

        /**
         * @brief Get the local address information for the socket
         *
         * @return A constant reference to the AddressInfo object containing the local address information.
         */
        [[nodiscard]] AddressInfo const& local_address() const& {
            return m_local_address_info;
        }

        AddressInfo const& remote_address() && = delete;

        /**
         * @brief Get the remote address information for the socket.
         *
         * @return A constant reference to the AddressInfo object representing the remote address information.
         */
        [[nodiscard]] AddressInfo const& remote_address() const& {
            return m_remote_address_info;
        }
    };

    namespace detail {
        [[nodiscard]] AbstractSocket::OsSocketHandle
        initialize_server_socket(AddressFamily address_family, std::uint16_t port);
    }

    /**
     * @class ServerSocket
     * @brief Represents a server socket that listens for incoming connections
     *
     * This class inherits from AbstractSocket and provides functionality to create a server socket
     * and handle incoming connections. It starts a listening thread to actively accept incoming
     * connections and a callback function to handle the new client sockets.
     */
    class ServerSocket final : public AbstractSocket {
        friend class Sockets;

    private:
        std::jthread m_listen_thread;

        ServerSocket(AddressFamily address_family, std::uint16_t port, std::function<void(ClientSocket)> on_connect);

    public:
        ServerSocket(ServerSocket&& other) noexcept = default;
        ServerSocket& operator=(ServerSocket&& other) noexcept = default;

        ~ServerSocket();

        /**
         * @brief Stop the server listening thread.
         */
        void stop();
    };

    /**
     * @class ClientSocket
     * @brief Represents a client socket for communication
     *
     * This class extends the AbstractSocket class and provides additional functionality
     * for sending and receiving data over a network connection. It manages send and
     * receive tasks asynchronously and handles socket operations.
     */
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

            SendTask(std::promise<std::size_t> promise, std::vector<std::byte> data)
                : promise{ std::move(promise) },
                  data{ std::move(data) } { }
        };

        struct ReceiveTask {
            enum class Kind {
                Exact,
                MaxBytes,
            };

            std::promise<std::vector<std::byte>> promise;
            std::size_t max_num_bytes;
            Kind kind;
            std::chrono::steady_clock::time_point end_time;

            ReceiveTask(
                    std::promise<std::vector<std::byte>> promise,
                    std::size_t const max_num_bytes,
                    Kind const kind,
                    std::chrono::steady_clock::time_point const end_time
            )
                : promise{ std::move(promise) },
                  max_num_bytes{ max_num_bytes },
                  kind{ kind },
                  end_time{ end_time } { }
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
                // we have to ensure that running is set to false while holding the lock, otherwise we have a
                // race condition regarding the condition variables which can lead to the threads blocking indefinitely
                receive_tasks.apply([this](auto const&) { *running = false; });
                send_tasks.apply([this](auto const&) { *running = false; });
                data_received_condition_variable.notify_one();
                data_sent_condition_variable.notify_one();
            }

            void clear_queues();
        };

        static constexpr auto default_timeout =
                static_cast<std::chrono::steady_clock::duration>(std::chrono::seconds{ 1 });

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

        /**
         * @brief Checks if the socket is currently connected.
         *
         * This function returns a boolean value indicating whether the socket is currently connected or not.
         *
         * @return True if the socket is connected, false otherwise.
         */
        [[nodiscard]] bool is_connected() const {
            return m_shared_state->is_running();
        }

        // clang-format off
        /**
         * @brief Sends the given data through the socket.
         *
         * The send method sends the provided data through the socket. It returns a std::future<std::size_t> that
         * represents the amount of data that has been transmitted.
         *
         * If the data vector is empty, a SendError exception will be thrown.
         *
         * @param data The data to be sent through the socket.
         * @return A std::future<std::size_t> that represents the amount of data that has been transmitted.
         * @throws SendError If the data vector is empty.
         */
        [[nodiscard("discarding the return value may lead to the data to never be transmitted")]]
        std::future<std::size_t> send(std::vector<std::byte> data);

        /**
         * @brief Sends data over the socket
         *
         * This method sends the given values over the socket. It constructs a message package by
         * inserting the values into a message buffer. It then sends it using through the socket.
         *
         * @tparam std::integral... values The values to send
         * @return A std::future<std::size_t> that will hold the number of bytes sent
         */
        [[nodiscard("discarding the return value may lead to the data to never be transmitted")]]
        std::future<std::size_t> send(std::integral auto... values) {
            auto package = MessageBuffer{};
            (package << ... << values);
            return send(std::move(package));
        }

        /**
         * @brief Sends data over the socket.
         *
         * This method sends the given message over the socket.
         *
         * The send method returns a std::future<std::size_t> that represents the amount of data that has been
         * transmitted.
         *
         * If the data vector is empty, a SendError exception will be thrown.
         *
         * @param package The message package to be sent through the socket.
         * @return A std::future<std::size_t> that represents the amount of data that has been transmitted.
         * @throws SendError If the data vector is empty.
         */
        [[nodiscard("discarding the return value may lead to the data to never be transmitted")]]
        std::future<std::size_t> send(MessageBuffer const& package) {
            return send(package.data());
        }

        /**
         * @brief Sends data over the socket.
         *
         * This method sends the given message over the socket.
         *
         * The send method returns a std::future<std::size_t> that represents the amount of data that has been
         * transmitted.
         *
         * If the data vector is empty, a SendError exception will be thrown.
         *
         * @param package The message package to be sent through the socket.
         * @return A std::future<std::size_t> that represents the amount of data that has been transmitted.
         * @throws SendError If the data vector is empty.
         */
        [[nodiscard("discarding the return value may lead to the data to never be transmitted")]]
        std::future<std::size_t> send(MessageBuffer&& package) {
            return send(std::move(package).data());
        }
        // clang-format on

        /**
         * @brief Asynchronously receives up to a specified maximum number of bytes from the socket
         *
         * This method receives up to a specified number of bytes from the socket asynchronously.
         * It returns an std::future object that can be used to retrieve the received data.
         *
         * @param max_num_bytes The maximum number of bytes to receive from the socket
         *
         * @return An std::future object representing the asynchronous receive operation.
         *         The future will hold an std::vector<std::byte> containing the received bytes.
         *         If no data is received within the default timeout of 1 second, an exception
         *         will be stored in the future.
         */
        [[nodiscard]] std::future<std::vector<std::byte>> receive(std::size_t max_num_bytes);

        /**
         * @brief Asynchronously receives up to a specified maximum number of bytes from the socket
         *
         * This method receives up to a specified number of bytes from the socket asynchronously.
         * It returns an std::future object that can be used to retrieve the received data.
         *
         * @param max_num_bytes The maximum number of bytes to receive from the socket
         * @param timeout The maximum amount of time to wait for incoming data
         *
         * @return An std::future object representing the asynchronous receive operation.
         *         The future will hold an std::vector<std::byte> containing the received bytes.
         *         If no data is received within the specified timeout, an exception will be stored
         *         in the future.
         */
        [[nodiscard]] std::future<std::vector<std::byte>> receive(std::size_t max_num_bytes, Timeout timeout);

        /**
         * @brief Receives a specified number of bytes from the client socket
         *
         * This method receives exactly the specified number of bytes from the client socket. It returns a
         * future object that will be fulfilled with a vector of bytes containing the received data.
         *
         * @param num_bytes The number of bytes to receive
         * @return A future that holds the received bytes as a vector of std::byte. If the operation
         *         cannot be completed within the default timeout of 1 second, an exception will be stored
         *         in the future.
         */
        [[nodiscard]] std::future<std::vector<std::byte>> receive_exact(std::size_t num_bytes);

        /**
         * @brief Receives a specified number of bytes from the socket
         *
         * This method receives exactly the specified number of bytes from the client socket. It returns a
         * future object that will be fulfilled with a vector of bytes containing the received data.
         *
         * @param num_bytes The number of bytes to receive
         * @param timeout The timeout for the receive operation
         * @return A future that holds the received bytes as a vector of std::byte. If the operation
         *         cannot be completed within the specified timeout, an exception will be stored in the
         *         future.
         */
        [[nodiscard]] std::future<std::vector<std::byte>> receive_exact(std::size_t num_bytes, Timeout timeout);

        /**
         * Reads one or multiple integral values from the socket.
         *
         * @tparam Ts std::integral... The types of the values to read from the socket
         * @param timeout The timeout for the receive operation
         * @return A future that holds either the read value, if only one type parameter was provided, or
         *         a tuple of all the read values according to the provided types. If the operation cannot
         *         be completed within the specified timeout, an exception will be stored in the future.
         */
        template<std::integral... Ts>
        [[nodiscard]] auto receive(Timeout const timeout = default_timeout) {
            static constexpr auto total_size = detail::summed_sizeof<Ts...>();
            auto future = receive_exact(total_size, timeout);
            return std::async(std::launch::deferred, [future = std::move(future)]() mutable {
                auto message_buffer = MessageBuffer{ future.get() };
                assert(message_buffer.size() == total_size);
                return message_buffer.try_extract<Ts...>().value(); // should never fail since we have enough data
            });
        }

        /**
         * @brief Closes the client socket
         *
         * This method closes the client socket and performs necessary cleanup operations. It stops
         * the socket from running and clears any pending queues.
         */
        void close();

    private:
        // clang-format off
        [[nodiscard]] std::future<std::vector<std::byte>> receive_implementation(
            std::size_t max_num_bytes,
            ReceiveTask::Kind kind,
            std::optional<std::chrono::steady_clock::time_point> end_time
        );
        // clang-format on
        [[nodiscard]] static bool process_receive_task(OsSocketHandle socket, ReceiveTask task);
        [[nodiscard]] static bool process_send_task(OsSocketHandle socket, SendTask task);
    };
} // namespace c2k
