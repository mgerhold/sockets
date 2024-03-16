#pragma once

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
    /**
     * @brief A thread-safe wrapper for data synchronization.
     *
     * This class provides a thread-safe wrapper for data synchronization. It allows multiple
     * threads to safely access and modify shared data.
     *
     * @tparam T The type of the data to be synchronized.
     */
    class Synchronized final {
    private:
        std::recursive_mutex m_mutex;
        T m_data;

    public:
        using value_type = T;

        explicit Synchronized(T data) : m_data{ std::move(data) } { }

        /**
         * @brief Applies a function to the synchronized data.
         *
         * This method applies a function to the synchronized data, ensuring thread safety by acquiring a lock
         * on the underlying mutex before invoking the function.
         *
         * @param function The function to be applied to the synchronized data.
         * @return The result of applying the function to the synchronized data.
         */
        auto apply(std::invocable<T&> auto&& function) {
            auto lock = std::scoped_lock{ m_mutex };
            return function(m_data);
        }

        /**
         * @brief Applies a function to the synchronized data.
         *
         * This method applies a function to the synchronized data, ensuring thread safety by acquiring a lock
         * on the underlying mutex before invoking the function.
         *
         * @param function The function to be applied to the synchronized data.
         * @return The result of applying the function to the synchronized data.
         */
        auto apply(std::invocable<T const&> auto&& function) const {
            auto lock = std::scoped_lock{ m_mutex };
            return function(m_data);
        }

        /**
         * @brief Waits until a condition is met on the synchronized data.
         *
         * This method waits until a specified condition is met on the synchronized data by using a condition variable.
         * It acquires a lock on the underlying mutex before waiting, ensuring thread safety.
         *
         * @param condition_variable The condition variable to wait on.
         * @param predicate The predicate that defines the condition to be met.
         */
        void wait(std::condition_variable_any& condition_variable, detail::Predicate<T&> auto&& predicate) {
            auto lock = std::unique_lock{ m_mutex };
            condition_variable.wait(lock, [&] { return predicate(m_data); });
        }

        /**
         * @brief Waits until a condition is met on the synchronized data.
         *
         * This method waits until a specified condition is met on the synchronized data by using a condition variable.
         * It acquires a lock on the underlying mutex before waiting, ensuring thread safety.
         *
         * @param condition_variable The condition variable to wait on.
         * @param predicate The predicate that defines the condition to be met.
         */
        void wait(std::condition_variable_any& condition_variable, detail::Predicate<T const&> auto&& predicate) const {
            auto lock = std::unique_lock{ m_mutex };
            condition_variable.wait(lock, [&] { return predicate(m_data); });
        }

        /**
         * @brief Waits until a specified condition is met on the synchronized data and applies a function to it.
         *
         * This method waits until a specified condition is met on the synchronized data by using a condition variable.
         * It acquires a lock on the underlying mutex before waiting, ensuring thread safety.
         * Once the condition is met, the provided function is applied to the synchronized data and the result is
         * returned (if there is any).
         *
         * @param condition_variable The condition variable to wait on.
         * @param predicate The predicate that defines the condition to be met.
         * @param function The function to be applied to the synchronized data.
         * @return The result of applying the function to the synchronized data (if any).
         */
        auto wait_and_apply(
                std::condition_variable_any& condition_variable,
                detail::Predicate<T&> auto&& predicate,
                std::invocable<T&> auto&& function
        ) {
            auto lock = std::unique_lock{ m_mutex };
            condition_variable.wait(lock, [&] { return predicate(m_data); });
            return function(m_data);
        }

        /**
         * @brief Waits until a specified condition is met on the synchronized data and applies a function to it.
         *
         * This method waits until a specified condition is met on the synchronized data by using a condition variable.
         * It acquires a lock on the underlying mutex before waiting, ensuring thread safety.
         * Once the condition is met, the provided function is applied to the synchronized data and the result is
         * returned (if there is any).
         *
         * @param condition_variable The condition variable to wait on.
         * @param predicate The predicate that defines the condition to be met.
         * @param function The function to be applied to the synchronized data.
         * @return The result of applying the function to the synchronized data (if any).
         */
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
