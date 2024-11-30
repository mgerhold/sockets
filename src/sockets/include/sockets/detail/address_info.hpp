#pragma once

#include <lib2k/unreachable.hpp>
#include <cstdint>
#include <iostream>

namespace c2k {
    /**
     * @struct AddressInfo
     * @brief Represents information about an address.
     *
     * This structure contains information about an address, including the address family, address string, and port number.
     */
    struct AddressInfo {
        /**
         * @enum AddressFamily
         * @brief Represents the address family of an address.
         *
         * This enum class defines the possible address families, such as IPv4, IPv6, and unspecified.
         */
        AddressFamily family = AddressFamily::Unspecified;

        /**
         * @var std::string address
         * @brief Represents the address string, e.g. "127.0.0.1" or "localhost".
         */
        std::string address;

        /**
         * @brief Represents a port number.
         */
        std::uint16_t port;

        /**
         * @relates AddressInfo
         * @brief Overload of the output stream operator for AddressInfo objects.
         *
         * This operator outputs the information about an AddressInfo object to the specified output stream.
         * The output format depends on the address family of the AddressInfo object.
         *
         * @param ostream The output stream.
         * @param address_info The AddressInfo object to be outputted.
         *
         * @return The output stream after the AddressInfo object has been outputted.
         */
        friend std::ostream& operator<<(std::ostream& ostream, AddressInfo const& address_info) {
            switch (address_info.family) {
                case AddressFamily::Unspecified:
                    return ostream << "<unspecified address family>";
                case AddressFamily::Ipv4:
                    return ostream << address_info.address << ':' << address_info.port;
                case AddressFamily::Ipv6:
                    return ostream << '[' << address_info.address << "]:" << address_info.port;
            }
            unreachable();
        }
    };
} // namespace c2k
