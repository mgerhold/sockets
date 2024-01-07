#pragma once

#include <cstdint>
#include <iostream>
#include <utility>

namespace c2k {
    struct AddressInfo {
        AddressFamily family = AddressFamily::Unspecified;
        std::string address;
        std::uint16_t port;

        friend std::ostream& operator<<(std::ostream& ostream, AddressInfo const& address_info) {
            switch (address_info.family) {
                case AddressFamily::Unspecified:
                    return ostream << "<unspecified address family>";
                case AddressFamily::Ipv4:
                    return ostream << address_info.address << ':' << address_info.port;
                case AddressFamily::Ipv6:
                    return ostream << '[' << address_info.address << "]:" << address_info.port;
            }
            std::unreachable();
        }
    };
} // namespace c2k
