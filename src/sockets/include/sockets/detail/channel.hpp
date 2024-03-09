#pragma once

#include <atomic>
#include <cassert>
#include <expected>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>

namespace c2k {

    /**
     * \brief Exception class for channel-related errors.
     *
     * This class is a subclass of std::runtime_error and is used to throw exceptions
     * when there are errors related to channels.
     */
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

    /**
     * \brief Class for sending values through a channel.
     */
    template<typename T>
    class Sender final : public detail::ChannelBase<T> {
        friend std::pair<Sender, Receiver<T>> create_channel<T>();

    private:
        using detail::ChannelBase<T>::ChannelBase;

    public:
        /**
         * \brief Sends a value through the channel. The call may block until the value can be sent.
         *
         * \param value The value to be sent through the channel.
         * \throw ChannelError If the channel has already been closed or if send() is called on a moved-from channel.
         */
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


        /**
         * \brief Attempts to send a value through the channel without blocking.
         *
         * This method attempts to send a value through the channel without blocking. If the channel has already been
         * closed or if try_send() is called on a moved-from channel, a ChannelError exception is thrown.
         *
         * \param value The value to be sent through the channel.
         * \return True if the value was successfully sent, false otherwise.
         * \throws ChannelError If the channel has already been closed or if try_send() is called on a moved-from
         *                      channel.
         */
        [[nodiscard]] bool try_send(T value) {
            if (this->state() == nullptr) {
                throw ChannelError{ "cannot call try_send() on a moved-from channel" };
            }
            auto lock = std::unique_lock{ this->state()->mutex };
            if (not this->state()->is_open or this->state()->value.has_value()) {
                return false;
            }
            assert(not this->state()->value.has_value());
            this->state()->value = std::move(value);
            this->state()->condition_variable.notify_one();
            return true;
        }
    };

    /**
     * \brief A class that represents a receiver in a channel.
     */
    template<typename T>
    class Receiver final : public detail::ChannelBase<T> {
        friend std::pair<Sender<T>, Receiver> create_channel<T>();

    private:
        using detail::ChannelBase<T>::ChannelBase;

    public:
        /**
         * \brief Receives a value from the channel. This call blocks until a value is received.
         *
         * \return The value received from the channel.
         * \throws ChannelError Thrown when the channel has already been moved from or closed.
         */
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
            this->state()->value.reset();
            this->state()->condition_variable.notify_one();
            return result;
        }

        /**
         * \brief Attempts to receive a value from the channel.
         *
         * This method tries to receive a value from the channel and does not block if no value is available.
         * If the channel has been moved from or closed, a ChannelError exception is thrown.
         *
         * \return An optional containing the received value if available, or std::nullopt if no value is available.
         * \throws ChannelError Thrown when the channel has already been moved from or closed.
         */
        [[nodiscard]] std::optional<T> try_receive() {
            if (this->state() == nullptr) {
                throw ChannelError{ "cannot call try_receive() on a moved-from channel" };
            }
            auto lock = std::unique_lock{ this->state()->mutex };
            if (not this->state()->is_open or not this->state()->value.has_value()) {
                return std::nullopt;
            }
            assert(this->state()->value.has_value());
            auto result = std::move(this->state()->value.value());
            this->state()->value.reset();
            this->state()->condition_variable.notify_one();
            return result;
        }
    };

    /**
     * \brief Creates a channel for sending and receiving data.
     *
     * This function creates and returns a pair consisting of a sender and a receiver for
     * the specified data type T.
     *
     * \tparam T The type of data to be sent and received.
     * \return A pair of Sender and Receiver objects.
     */
    template<typename T>
    [[nodiscard]] std::pair<Sender<T>, Receiver<T>> create_channel() {
        auto state = std::make_shared<detail::ChannelState<T>>();

        auto sender = Sender<T>{ state }; // state is copied
        auto receiver = Receiver<T>{ std::move(state) };

        return { std::move(sender), std::move(receiver) };
    }

    template<typename T>
    class BidirectionalChannel;

    template<typename T>
    [[nodiscard]] std::pair<BidirectionalChannel<T>, BidirectionalChannel<T>> create_bidirectional_channel_pair();

    /**
     * \brief A bidirectional communication channel that allows sending and receiving data from both ends.
     */
    template<typename T>
    class BidirectionalChannel final {
        friend std::pair<BidirectionalChannel, BidirectionalChannel> create_bidirectional_channel_pair<T>();

    private:
        Sender<T> m_sender;
        Receiver<T> m_receiver;

        BidirectionalChannel(Sender<T> sender, Receiver<T> receiver)
            : m_sender{ std::move(sender) },
              m_receiver{ std::move(receiver) } { }


    public:
        /**
         * \brief Sends a value using the m_sender object.
         *
         * \param value The value to be sent.
         * \throws ChannelError If the channel has already been closed or if try_send() is called on a moved-from
         *                      channel.
         */
        void send(T value) {
            m_sender.send(std::move(value));
        }


        /**
         * \brief Attempts to send a value through the channel without blocking.
         *
         * \param value The value to be sent through the channel.
         * \return True if the value was successfully sent, false otherwise.
         * \throws ChannelError If the channel has already been closed or if try_send() is called on a moved-from
         *                      channel.
         */
        [[nodiscard]] bool try_send(T value) {
            return m_sender.try_send(std::move(value));
        }

        /**
         * \brief Receive a value from the channel. This function blocks until a value is received from the channel.
         *
         * \tparam T The type of the value to receive.
         * \return The received value.
         * \throws ChannelError If the channel has already been closed or if try_send() is called on a moved-from
         *                      channel.
         */
        [[nodiscard]] T receive() {
            return m_receiver.receive();
        }

        /**
         * \brief Attempts to receive a value from the channel without blocking.
         *
         * \tparam T The type of the value to receive.
         * \return An optional containing the received value if available, or std::nullopt if no value is available.
         * \throws ChannelError Thrown when the channel has already been moved from or closed.
         */
        [[nodiscard]] std::optional<T> try_receive() {
            return m_receiver.try_receive();
        }

        /**
         * \brief Checks if the channel is open.
         *
         * \return True if the channel is open, false otherwise.
         */
        [[nodiscard]] bool is_open() const {
            return m_sender.is_open() and m_receiver.is_open();
        }
    };

    /**
     * \brief Creates a pair of bidirectional channels.
     *
     * This function creates a pair of bidirectional channels. It returns an `std::pair` containing two instances of
     * `BidirectionalChannel<T>`, where `T` is the type of the channel values.
     *
     * \tparam T The type of the channel values.
     * \return A `std::pair` containing two instances of `BidirectionalChannel<T>`.
     */
    template<typename T>
    [[nodiscard]] std::pair<BidirectionalChannel<T>, BidirectionalChannel<T>> create_bidirectional_channel_pair() {
        auto [sender_a, receiver_a] = create_channel<T>();
        auto [sender_b, receiver_b] = create_channel<T>();

        auto channel_a = BidirectionalChannel<T>{ std::move(sender_a), std::move(receiver_b) };
        auto channel_b = BidirectionalChannel<T>{ std::move(sender_b), std::move(receiver_a) };
        return { std::move(channel_a), std::move(channel_b) };
    }
} // namespace c2k
