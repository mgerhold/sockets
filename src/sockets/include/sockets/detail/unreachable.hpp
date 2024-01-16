#pragma once

#include <version>
#ifdef __cpp_lib_unreachable
#include <utility>
#endif

namespace c2k {
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
