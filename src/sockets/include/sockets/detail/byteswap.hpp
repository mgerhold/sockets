#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>

/**
 * @brief Swap the byte order of a value.
 *
 * This function swaps the byte order of a given value.
 * If the compiler supports `std::byteswap`, the function uses it.
 * Otherwise, it reverses the byte representation manually.
 *
 * @tparam T The type of the value.
 * @param value The value to swap the byte order of.
 * @return The value with the byte order swapped.
 */
template<std::integral T>
constexpr T byteswap(T value) noexcept {
#ifdef __cpp_lib_byteswap
    return std::byteswap(value);
#else
    static_assert(std::has_unique_object_representations_v<T>, "T may not have padding bits");
    auto value_representation = std::bit_cast<std::array<std::byte, sizeof(T)> >(value);
    std::reverse(value_representation.begin(), value_representation.end());
    return std::bit_cast<T>(value_representation);
#endif
}
