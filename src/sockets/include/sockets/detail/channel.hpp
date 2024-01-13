#pragma once

#include <atomic>
#include <cassert>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>

namespace c2k {

    class ChannelError final : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    template<typename T>
    class Sender;

    template<typename T>
    class Receiver;

    template<typename T>
    std::pair<Sender<T>, Receiver<T>> create_channel();

    namespace detail {
        template<typename T>
        struct ChannelState {
            std::mutex mutex;
            std::atomic_bool is_open{ true };
            std::condition_variable condition_variable;
            std::optional<T> value;
        };

        template<typename T>
        class ChannelBase {
        protected:
            std::shared_ptr<ChannelState<T>> m_state;

            explicit ChannelBase(std::shared_ptr<ChannelState<T>> state) : m_state{ std::move(state) } { }

        public:
            ChannelBase(ChannelBase const& other) = delete;
            ChannelBase(ChannelBase&& other) noexcept = default;
            ChannelBase& operator=(ChannelBase const& other) = delete;
            ChannelBase& operator=(ChannelBase&& other) noexcept = default;

            ~ChannelBase() {
                if (state() != nullptr) {
                    auto lock = std::scoped_lock{ state()->mutex };
                    state()->is_open = false;
                }
            }

            [[nodiscard]] bool is_open() const {
                if (m_state == nullptr) {
                    return false;
                }
                auto lock = std::scoped_lock{ m_state->mutex };
                return m_state->is_open;
            }

        protected:
            [[nodiscard]] std::shared_ptr<ChannelState<T>> const& state() const {
                return m_state;
            }

            [[nodiscard]] std::shared_ptr<ChannelState<T>>& state() {
                return m_state;
            }
        };
    } // namespace detail

    template<typename T>
    class Sender final : public detail::ChannelBase<T> {
        friend std::pair<Sender, Receiver<T>> create_channel<T>();

    private:
        using detail::ChannelBase<T>::ChannelBase;

    public:
        void send(T value) {
            if (this->state() == nullptr) {
                throw ChannelError{ "cannot call send() on a moved-from channel" };
            }
            auto lock = std::unique_lock{ this->state()->mutex };
            this->state()->condition_variable.wait(lock, [this]() {
                return not this->state()->is_open or not this->state()->value.has_value();
            });
            if (not this->state()->is_open) {
                throw ChannelError{ "channel has already closed" };
            }
            assert(not this->state()->value.has_value());
            this->state()->value = std::move(value);
            this->state()->condition_variable.notify_one();
        }
    };

    template<typename T>
    class Receiver final : public detail::ChannelBase<T> {
        friend std::pair<Sender<T>, Receiver> create_channel<T>();

    private:
        using detail::ChannelBase<T>::ChannelBase;

    public:
        [[nodiscard]] T receive() {
            if (this->state() == nullptr) {
                throw ChannelError{ "cannot call receive() on a moved-from channel" };
            }
            auto lock = std::unique_lock{ this->state()->mutex };
            this->state()->condition_variable.wait(lock, [this]() {
                return this->state()->value.has_value() or not this->state()->is_open;
            });
            if (not this->state()->value.has_value() and not this->state()->is_open) {
                throw ChannelError{ "channel has already closed" };
            }
            assert(this->state()->value.has_value());
            auto result = std::move(this->state()->value.value());
            this->state()->value = std::nullopt;
            this->state()->condition_variable.notify_one();
            return result;
        }
    };

    template<typename T>
    [[nodiscard]] std::pair<Sender<T>, Receiver<T>> create_channel() {
        auto state = std::make_shared<detail::ChannelState<T>>();

        auto sender = Sender<T>{ state }; // state is copied
        auto receiver = Receiver<T>{ std::move(state) };

        return { std::move(sender), std::move(receiver) };
    }
} // namespace c2k
