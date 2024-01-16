#pragma once

#include "byteswap.hpp"
#include <bit>

namespace c2k {
    inline constexpr auto network_byte_order = std::endian::big; // for IP communication

    [[nodiscard]] constexpr auto to_network_byte_order(std::integral auto const value) {
        if constexpr (std::endian::native == network_byte_order) {
            return value;
        } else {
            return byteswap(value);
        }
    }

    [[nodiscard]] constexpr auto from_network_byte_order(std::integral auto const value) {
        return to_network_byte_order(value);
    }
} // namespace c2k
