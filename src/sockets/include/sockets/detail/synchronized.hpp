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

    /*template<typename T, typename Returner>
    class Locked final {
        friend class Synchronized<T>;

    private:
        std::unique_ptr<T> m_data;
        Returner m_returner;

        Locked(std::unique_ptr<T> data, Returner returner)
            : m_data{ std::move(data) },
              m_returner{ std::move(returner) } { }

        void throw_if_expired() const { }

    public:
        Locked(Locked const& other) = delete;
        Locked(Locked&& other) noexcept = default;
        Locked& operator=(Locked const& other) = delete;
        Locked& operator=(Locked&& other) noexcept = default;

        ~Locked() {
            m_returner(std::move(*m_data));
        }

        [[nodiscard]] T const& value() const& {
            throw_if_expired();
            return *m_data;
        }

        [[nodiscard]] T& value() & {
            throw_if_expired();
            return *m_data;
        }

        T const& operator*() && = delete;

        [[nodiscard]] T const& operator*() const& {
            throw_if_expired();
            return value();
        }

        [[nodiscard]] T& operator*() & {
            throw_if_expired();
            return value();
        }

        T const* operator->() && = delete;

        [[nodiscard]] T const* operator->() const& {
            return m_data.get();
        }

        [[nodiscard]] T* operator->() & {
            return m_data.get();
        }
    };*/

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
