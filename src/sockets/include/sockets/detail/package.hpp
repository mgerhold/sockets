#pragma once

#include "byte_order.hpp"
#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace c2k {
    template<std::size_t length = 0>
    struct Package final {
        std::array<std::byte, length> data{};

        Package() = default;
        explicit Package(std::array<std::byte, length> data_) : data{ data_ } { }

        template<std::integral T>
        friend constexpr auto operator<<(Package const package, T const value) {
            static constexpr auto new_length = length + sizeof(value);
            auto result = Package<new_length>{};
            std::copy_n(package.data.cbegin(), package.data.size(), result.data.begin());
            auto const network_value = to_network_byte_order(value);
            std::memcpy(result.data.data() + length, &network_value, sizeof(network_value));
            return result;
        }
    };
} // namespace c2k
