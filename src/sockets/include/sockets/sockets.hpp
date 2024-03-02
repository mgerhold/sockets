#pragma once

#include "detail/address_family.hpp"
#include "detail/address_info.hpp"
#include "detail/byte_order.hpp"
#include "detail/extractor.hpp"
#include "detail/message_buffer.hpp"
#include "detail/non_null_owner.hpp"
#include "detail/socket.hpp"
#include "detail/synchronized.hpp"
#include "detail/unique_value.hpp"

namespace c2k {
    class Sockets final {
    private:
        Sockets();

    public:
        Sockets(Sockets const& other) = delete;
        Sockets(Sockets&& other) noexcept = delete;
        Sockets& operator=(Sockets const& other) = delete;
        Sockets& operator=(Sockets&& other) noexcept = delete;
        ~Sockets();

        // clang-format off
        [[nodiscard]] static ServerSocket create_server(
            AddressFamily const address_family,
            std::uint16_t const port,
            std::function<void(ClientSocket)> callback,
            Sockets const& = instance()
        ) {
            return ServerSocket{ address_family, port, std::move(callback) };
        }

        [[nodiscard]] static ClientSocket create_client(
            AddressFamily address_family,
            std::string const& host,
            std::uint16_t port,
            Sockets const& = instance()
        );
        // clang-format on

    private:
        [[nodiscard]] static Sockets const& instance();
    };
} // namespace c2k
