#pragma once

#include <version>
#ifdef __cpp_lib_unreachable
#include <utility>
#endif

namespace c2k {
    /**
     * @brief The `unreachable` function is used to mark a point in the code as unreachable.
     *
     * @details The `unreachable` function uses conditional compilation to invoke the appropriate
     * mechanism for marking the code as unreachable. If the compiler supports the `__cpp_lib_unreachable`
     * feature defined in the C++17 standard, it uses `std::unreachable()`. Otherwise, it falls back to
     * compiler-specific mechanisms. For MSVC, it uses `__assume(false)`, and for GCC and Clang, it uses
     * `__builtin_unreachable()`.
     */
    [[noreturn]] inline void unreachable() {
#ifdef __cpp_lib_unreachable
        std::unreachable();
#else
#if defined(_MSC_VER) && !defined(__clang__) // MSVC
        __assume(false);
#else                                        // GCC, Clang
        __builtin_unreachable();
#endif
#endif
    }
} // namespace c2k
