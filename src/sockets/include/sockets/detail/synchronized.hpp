#pragma once

#include "channel.hpp"
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <variant>
#include <vector>

namespace c2k {
    template<typename T>
    class Synchronized;

    namespace detail {
        template<typename Func, typename Arg>
        concept Predicate = requires(Func func, Arg arg) {
            { func(arg) } -> std::same_as<bool>;
        };
    } // namespace detail

    template<typename T>
    class Synchronized final {
    private:
        std::recursive_mutex m_mutex;
        T m_data;

    public:
        using value_type = T;

        explicit Synchronized(T data) : m_data{ std::move(data) } { }

        auto apply(std::invocable<T&> auto&& function) {
            auto lock = std::scoped_lock{ m_mutex };
            return function(m_data);
        }

        auto apply(std::invocable<T const&> auto&& function) const {
            auto lock = std::scoped_lock{ m_mutex };
            return function(m_data);
        }

        void wait(std::condition_variable_any& condition_variable, detail::Predicate<T&> auto&& predicate) {
            auto lock = std::unique_lock{ m_mutex };
            condition_variable.wait(lock, [&] { return predicate(m_data); });
        }

        void wait(std::condition_variable_any& condition_variable, detail::Predicate<T const&> auto&& predicate) const {
            auto lock = std::unique_lock{ m_mutex };
            condition_variable.wait(lock, [&] { return predicate(m_data); });
        }

        auto wait_and_apply(
                std::condition_variable_any& condition_variable,
                detail::Predicate<T&> auto&& predicate,
                std::invocable<T&> auto&& function
        ) {
            auto lock = std::unique_lock{ m_mutex };
            condition_variable.wait(lock, [&] { return predicate(m_data); });
            return function(m_data);
        }

        auto wait_and_apply(
                std::condition_variable_any& condition_variable,
                detail::Predicate<T const&> auto&& predicate,
                std::invocable<T const&> auto&& function
        ) const {
            auto lock = std::unique_lock{ m_mutex };
            condition_variable.wait(lock, [&] { return predicate(m_data); });
            return function(m_data);
        }
    };
} // namespace c2k
