#pragma once

#include "byteswap.hpp"
#include <bit>

namespace c2k {
    inline constexpr auto network_byte_order = std::endian::big; // for IP communication

    /**
     * @brief Converts a value to network byte order.
     *
     * This function converts the given value to network byte order if the
     * native byte order is different from network byte order. Otherwise,
     * it returns the value as is.
     *
     * @tparam T The integral type of the value.
     * @param value The value to convert.
     * @return The converted value in network byte order.
     */
    [[nodiscard]] constexpr auto to_network_byte_order(std::integral auto const value) {
        if constexpr (std::endian::native == network_byte_order) {
            return value;
        } else {
            return byteswap(value);
        }
    }

    /**
     * @brief Converts a value from network byte order to native byte order.
     *
     * This function converts the given value from network byte order to
     * native byte order if the native byte order is different from network
     * byte order. Otherwise, it returns the value as is.
     *
     * @tparam T The integral type of the value.
     * @param value The value to convert.
     * @return The converted value.
     */
    [[nodiscard]] constexpr auto from_network_byte_order(std::integral auto const value) {
        return to_network_byte_order(value);
    }
} // namespace c2k
