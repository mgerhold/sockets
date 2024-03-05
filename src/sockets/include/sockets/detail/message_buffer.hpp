#pragma once

#include "byte_order.hpp"
#include "utils.hpp"
#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

namespace c2k {
    class MessageBuffer final {
    private:
        std::vector<std::byte> m_data{};

    public:
        MessageBuffer() = default;
        explicit MessageBuffer(std::vector<std::byte> data) : m_data{ std::move(data) } { }

        [[nodiscard]] std::size_t size() const {
            return m_data.size();
        }

        [[nodiscard]] std::vector<std::byte> const& data() const& {
            return m_data;
        }

        [[nodiscard]] std::vector<std::byte> data() && {
            return std::move(m_data);
        }

        template<std::integral T>
        friend MessageBuffer& operator<<(MessageBuffer& package, T const value) {
            auto buffer = std::array<std::byte, sizeof(value)>{};
            auto const network_value = to_network_byte_order(value);
            std::copy_n(reinterpret_cast<std::byte const*>(&network_value), sizeof(network_value), buffer.data());
            package.m_data.insert(package.m_data.end(), buffer.cbegin(), buffer.cend());
            return package;
        }

        template<std::integral T>
        friend MessageBuffer&& operator<<(MessageBuffer&& package, T const value) {
            package << value;
            return std::move(package);
        }

        template<std::integral... Ts>
        [[nodiscard]] auto try_extract() {
            static_assert(sizeof...(Ts) > 0, "at least one type argument must be specified");
            if constexpr (sizeof...(Ts) == 1) {
                if (size() < detail::summed_sizeof<Ts...>()) {
                    return std::optional<Ts...>{ std::nullopt };
                }
                auto result = (Ts{}, ...);
                *this >> result;
                return std::optional<Ts...>{ result };
            } else {
                if (size() < detail::summed_sizeof<Ts...>()) {
                    return std::optional<std::tuple<Ts...>>{};
                }
                return std::optional{ std::tuple<Ts...>{ [&] {
                    auto value = Ts{};
                    *this >> value;
                    return value;
                }()... } };
            }
        }

        friend MessageBuffer& operator>>(MessageBuffer& message_buffer, std::integral auto& target) {
            if (message_buffer.m_data.size() < sizeof(target)) {
                throw std::runtime_error{ "not enough data to extract value" };
            }
            auto buffer = std::remove_cvref_t<decltype(target)>{};
            std::copy_n(message_buffer.m_data.cbegin(), sizeof(target), reinterpret_cast<std::byte*>(&buffer));
            target = from_network_byte_order(buffer);
            message_buffer.m_data.erase(
                    message_buffer.m_data.cbegin(),
                    message_buffer.m_data.cbegin() + sizeof(target)
            );
            return message_buffer;
        }

        friend MessageBuffer& operator<<(MessageBuffer& message_buffer, std::span<std::byte const> const data) {
            message_buffer.m_data.insert(message_buffer.m_data.end(), data.begin(), data.end());
            return message_buffer;
        }
    };
} // namespace c2k
