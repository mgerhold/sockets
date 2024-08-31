#pragma once

#include "byte_order.hpp"
#include "utils.hpp"
#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace c2k {
    /**
     * @brief A class for storing and manipulating message data.
     *
     * The MessageBuffer class provides operations to store and manipulate message data.
     * It allows appending data to the buffer, extracting values from the buffer, and
     * providing access to the underlying buffer data. It automatically handles converting
     * between network byte order and native byte order.
     */
    class MessageBuffer final {
    private:
        std::vector<std::byte> m_data{};

    public:
        /**
         * @brief Creates a MessageBuffer object with an empty data buffer.
         */
        MessageBuffer() = default;

        /**
         * @brief Creates a MessageBuffer object using the specified data.
         *
         * @param data The vector containing the data to be stored in the MessageBuffer.
         */
        explicit MessageBuffer(std::vector<std::byte> data) : m_data{ std::move(data) } { }

        /**
         * @brief Returns the size of the MessageBuffer.
         *
         * @return The size of the MessageBuffer in bytes.
         */
        [[nodiscard]] std::size_t size() const {
            return m_data.size();
        }

        /**
         * @brief Returns a reference to the underlying data vector.
         *
         * @return A const reference to the underlying data vector.
         */
        [[nodiscard]] std::vector<std::byte> const& data() const& {
            return m_data;
        }

        /**
         * @brief Returns the data vector by moving it.
         *
         * @return The data vector of the MessageBuffer object.
         */
        [[nodiscard]] std::vector<std::byte> data() && {
            return std::move(m_data);
        }

        /**
         * @brief Overloaded insertion operator for appending a value to a MessageBuffer.
         *
         * This operator allows appending a value of type T to a MessageBuffer object.
         * The value is first converted to network byte order, and then appended to the MessageBuffer's
         * internal data storage.
         *
         * @param message_buffer The MessageBuffer to append the value to.
         * @param value The value to be appended to the MessageBuffer.
         *
         * @return A reference to the modified MessageBuffer object.
         */
        template<std::integral T>
        friend MessageBuffer& operator<<(MessageBuffer& message_buffer, T const value) {
            auto buffer = std::array<std::byte, sizeof(value)>{};
            auto const network_value = to_network_byte_order(value);
            std::copy_n(reinterpret_cast<std::byte const*>(&network_value), sizeof(network_value), buffer.data());
            message_buffer.m_data.insert(message_buffer.m_data.end(), buffer.cbegin(), buffer.cend());
            return message_buffer;
        }

        /**
         * @brief Overloaded insertion operator for appending a value to a MessageBuffer.
         *
         * This operator allows appending a value of type T to a MessageBuffer object.
         * The value is first converted to network byte order, and then appended to the MessageBuffer's
         * internal data storage.
         *
         * @param message_buffer The MessageBuffer to append the value to.
         * @param value The value to be appended to the MessageBuffer.
         *
         * @return A reference to the modified MessageBuffer object.
         */
        template<std::integral T>
        friend MessageBuffer&& operator<<(MessageBuffer&& message_buffer, T const value) {
            message_buffer << value;
            return std::move(message_buffer);
        }

        /**
         * @brief Tries to extract values from the MessageBuffer.
         *
         * This function tries to extract values from the MessageBuffer based on the specified types. Converting
         * between network byte order and native byte order is handled automatically.
         * If the size of the MessageBuffer is not sufficient to extract all the specified types, an empty optional is
         * returned.
         *
         * @tparam Ts The types of the values to extract.
         * @return An optional containing the extracted value (if only one), an optional containing an std::tuple or
         *         of the requested values, or an empty optional if extraction fails.
         */
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

        /**
         * @brief Overloads the >> operator for extracting data from a MessageBuffer into an integral type.
         *
         * This function extracts a value from a MessageBuffer and stores it in the provided integral type,
         * after converting it from network byte order to native byte order. If the MessageBuffer does not
         * contain enough data to extract the value, a std::runtime_error is thrown.
         *
         * @param message_buffer The MessageBuffer from which to extract the value.
         * @param target The integral type variable in which to store the extracted value.
         *
         * @return A reference to the original MessageBuffer object after extraction.
         *
         * @throw std::runtime_error if not enough data is available in the MessageBuffer.
         */
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

        /**
         * @brief Appends the provided data to the message buffer.
         *
         * The `operator<<` function allows appending `data` to the `message_buffer`.
         *
         * @param message_buffer The message buffer to append data to.
         * @param data The data to append to the message buffer.
         * @return A reference to the `message_buffer` after the data has been appended.
         */
        friend MessageBuffer& operator<<(MessageBuffer& message_buffer, std::span<std::byte const> const data) {
            message_buffer.m_data.insert(message_buffer.m_data.end(), data.begin(), data.end());
            return message_buffer;
        }
    };
} // namespace c2k
