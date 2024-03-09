#pragma once

#include <cstdlib>

namespace c2k::detail {
    /**
     * @brief Calculate the combined size of multiple types.
     *
     * This function calculates and returns the sum of the sizes of the specified types.
     * The size is calculated using the sizeof operator.
     *
     * @tparam Ts The types whose sizes are to be summed.
     * @return The combined size of the types.
     *
     * @note This function is a consteval, which means it will be evaluated at compile-time.
     */
    template<typename... Ts>
    [[nodiscard]] consteval std::size_t summed_sizeof() {
        return (sizeof(Ts) + ...);
    }
} // namespace c2k::detail
