#pragma once

namespace c2k {
    /**
     * @enum AddressFamily
     * @brief Enum class representing different address families.
     *
     * The AddressFamily enum class represents different address families. Valid
     * values are Unspecified, Ipv4, and Ipv6.
     */
    enum class AddressFamily {
        Unspecified,
        Ipv4,
        Ipv6,
    };
}
