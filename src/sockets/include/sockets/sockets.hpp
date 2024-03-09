#pragma once

#include "detail/address_family.hpp"
#include "detail/address_info.hpp"
#include "detail/byte_order.hpp"
#include "detail/message_buffer.hpp"
#include "detail/non_null_owner.hpp"
#include "detail/socket.hpp"
#include "detail/synchronized.hpp"
#include "detail/unique_value.hpp"

namespace c2k {

    /**
     * @class Sockets
     * This class provides static member functions for creating server and client sockets.
     */
    class Sockets final {
    private:
        Sockets();

    public:
        Sockets(Sockets const& other) = delete;
        Sockets(Sockets&& other) noexcept = delete;
        Sockets& operator=(Sockets const& other) = delete;
        Sockets& operator=(Sockets&& other) noexcept = delete;
        ~Sockets();

        /**
         * @brief Creates a server socket.
         *
         * @param address_family The address family of the socket (Unspecified, Ipv4, Ipv6).
         * @param port The port number to bind the socket to (0 to let the operating system choose a socket).
         * @param callback The callback function to be executed when a client connects.
         * @param key The Sockets instance to use. Cannot be user-provided.
         *
         * @return The created ServerSocket object.
         */
        [[nodiscard]] static ServerSocket create_server(
                AddressFamily const address_family,
                std::uint16_t const port,
                std::function<void(ClientSocket)> callback,
                [[maybe_unused]] Sockets const& key = instance()
        ) {
            return ServerSocket{ address_family, port, std::move(callback) };
        }

        /**
         * @brief Creates a client socket.
         *
         * @param address_family The address family of the socket (Unspecified, Ipv4, Ipv6).
         * @param host The hostname or IP address of the server to connect to.
         * @param port The port number of the server to connect to.
         * @param key The Sockets instance to use. Cannot be user-provided.
         *
         * @return The created ClientSocket object.
         */
        [[nodiscard]] static ClientSocket create_client(
                AddressFamily address_family,
                std::string const& host,
                std::uint16_t port,
                [[maybe_unused]] Sockets const& key = instance()
        );

    private:
        [[nodiscard]] static Sockets const& instance();
    };

} // namespace c2k
