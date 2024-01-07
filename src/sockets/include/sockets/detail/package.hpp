#pragma once

#include "byte_order.hpp"
#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace c2k {
    struct Package final {
        std::vector<std::byte> data{};

        explicit Package(std::vector<std::byte> data_ = {}) : data{ std::move(data_) } { }

        template<std::integral T>
        friend Package& operator<<(Package& package, T const value) {
            auto buffer = std::array<std::byte, sizeof(value)>{};
            auto const network_value = to_network_byte_order(value);
            std::copy_n(reinterpret_cast<std::byte const*>(&network_value), sizeof(network_value), buffer.data());
            package.data.insert(package.data.end(), buffer.cbegin(), buffer.cend());
            return package;
        }
    };
} // namespace c2k
