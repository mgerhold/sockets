#pragma once

#include <cstdlib>

namespace c2k::detail {
    template<typename... Ts>
    [[nodiscard]] consteval std::size_t summed_sizeof() {
        return (sizeof(Ts) + ...);
    }
} // namespace c2k::detail
