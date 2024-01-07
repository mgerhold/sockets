#pragma once

#include "sockets/detail/byte_order.hpp"


#include <algorithm>
#include <concepts>
#include <cstddef>
#include <optional>
#include <span>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace c2k {
    class Extractor final {
    private:
        std::vector<std::byte> m_data{};

    public:
        Extractor() = default;
        explicit Extractor(std::vector<std::byte> data) : m_data{ std::move(data) } { }

        [[nodiscard]] std::size_t size() const {
            return m_data.size();
        }

        template<std::integral... Ts>
        [[nodiscard]] auto try_extract() {
            static_assert(sizeof...(Ts) > 0, "at least one type argument must be specified");
            if constexpr (sizeof...(Ts) == 1) {
                if (size() < sizeof...(Ts)) {
                    return std::optional<Ts...>{ std::nullopt };
                }
                auto result = (Ts{}, ...);
                *this >> result;
                return std::optional<Ts...>{ result };
            } else {
                if (size() < sizeof...(Ts)) {
                    return std::optional<std::tuple<Ts...>>{};
                }
                return std::optional{ std::tuple<Ts...>{ [&] {
                    auto value = Ts{};
                    *this >> value;
                    return value;
                }()... } };
            }
        }

        friend Extractor& operator>>(Extractor& extractor, std::integral auto& target) {
            if (extractor.m_data.size() < sizeof(target)) {
                throw std::runtime_error{ "not enough data to extract value" };
            }
            auto buffer = std::remove_cvref_t<decltype(target)>{};
            std::copy_n(extractor.m_data.cbegin(), sizeof(target), reinterpret_cast<std::byte*>(&buffer));
            target = from_network_byte_order(buffer);
            std::rotate(extractor.m_data.begin(), extractor.m_data.begin() + sizeof(target), extractor.m_data.end());
            extractor.m_data.resize(extractor.m_data.size() - sizeof(target));
            return extractor;
        }

        friend Extractor& operator<<(Extractor& extractor, std::span<std::byte const> const data) {
            extractor.m_data.insert(extractor.m_data.end(), data.cbegin(), data.cend());
            return extractor;
        }
    };
} // namespace c2k
