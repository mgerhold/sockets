#pragma once

#include <atomic>
#include <cassert>
#include <memory>
#include <mutex>
#include <vector>

template<typename T>
class Synchronized;

template<typename T>
class Locked final {
private:
    std::weak_ptr<std::monostate> m_weak_ptr;
    std::unique_lock<std::mutex> m_lock;
    std::thread::id m_initial_thread_id;
    T* m_data;

    friend class Synchronized<T>;

    Locked(std::weak_ptr<std::monostate> weak_ptr, std::unique_lock<std::mutex> lock, T* const data)
        : m_weak_ptr{ std::move(weak_ptr) },
          m_lock{ std::move(lock) },
          m_initial_thread_id{ std::this_thread::get_id() },
          m_data{ data } { }

    void throw_if_expired() const {
        if (std::this_thread::get_id() != m_initial_thread_id) {
            throw std::runtime_error{ "lock was moved across thread boundaries" };
        }
        if (m_weak_ptr.expired()) {
            throw std::runtime_error{ "locked object does no longer exist" };
        }
    }

public:
    T const& value() && = delete;

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
        return m_data;
    }

    [[nodiscard]] T* operator->() & {
        return m_data;
    }
};

template<typename T>
class Synchronized final {
private:
    std::mutex m_mutex;
    T m_data;
    std::shared_ptr<std::monostate> m_active_lock;

    friend class Locked<T>;

public:
    using value_type = T;

    explicit Synchronized(T data) : m_data{ std::move(data) } { }

    ~Synchronized() {
        auto lock = std::scoped_lock{ m_mutex };
    }

    [[nodiscard]] Locked<T> lock() {
        if (m_mutex.try_lock()) {
            m_active_lock = {};
            m_mutex.unlock();
        }
        auto lock = std::unique_lock{ m_mutex };
        m_active_lock = std::make_shared<std::monostate>();
        return Locked<T>{ std::weak_ptr{ m_active_lock }, std::move(lock), &m_data };
    }
};
