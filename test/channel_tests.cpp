#include "sockets/detail/channel.hpp"
#include <array>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

using namespace c2k;

TEST(ChannelTests, Instantiate) {
    auto [sender, receiver] = create_channel<int>();
}

TEST(ChannelTests, SendAndReceiveSingleValue) {
    auto [sender, receiver] = create_channel<int>();
    auto send_thread = std::jthread([sender_ = std::move(sender)]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        sender_.send(42);
    });
    auto const result = receiver.receive();
    ASSERT_EQ(result, 42);
}

TEST(ChannelTests, SendAndReceiveManyValues) {
    static constexpr auto num_values = std::size_t{ 500'000 };
    auto [sender, receiver] = create_channel<std::size_t>();
    auto send_thread = std::jthread([sender_ = std::move(sender)]() mutable {
        for (auto i = std::size_t{ 0 }; i < num_values; ++i) {
            sender_.send(i);
        }
    });
    for (auto i = std::size_t{ 0 }; i < num_values; ++i) {
        ASSERT_EQ(i, receiver.receive());
    }
}

TEST(ChannelTests, BidirectionalCommunication) {
    static constexpr auto num_values = 42'000;
    auto [sender_a, receiver_a] = create_channel<int>();
    auto [sender_b, receiver_b] = create_channel<int>();

    auto count = std::atomic_int{};
    static auto const lambda = [&count](Sender<int> sender, Receiver<int> receiver) {
        for (auto i = 0; i < num_values; ++i) {
            sender.send(i);
            ASSERT_EQ(i, receiver.receive());
            ++count;
        }
    };

    auto threads = std::array{
        std::jthread{ lambda, std::move(sender_a), std::move(receiver_b) },
        std::jthread{ lambda, std::move(sender_b), std::move(receiver_a) },
    };

    for (auto& thread : threads) {
        thread.join();
    }

    ASSERT_EQ(count, 2 * num_values);
}

TEST(ChannelTests, CannotUseClosedChannel) {
    {
        auto [sender, receiver] = create_channel<std::size_t>();
        [s = std::move(sender)] {}();
        EXPECT_THROW(sender.send(42), ChannelError);
        EXPECT_THROW(std::ignore = receiver.receive(), ChannelError);
    }
    {
        auto [sender, receiver] = create_channel<std::size_t>();
        [s = std::move(receiver)] {}();
        EXPECT_THROW(sender.send(42), ChannelError);
        EXPECT_THROW(std::ignore = receiver.receive(), ChannelError);
    }
}

TEST(ChannelTests, CanStillReceiveFromClosedChannel) {
    auto [sender, receiver] = create_channel<int>();
    [s_ = std::move(sender)]() mutable { s_.send(42); }();
    EXPECT_FALSE(receiver.is_open());
    ASSERT_EQ(receiver.receive(), 42);
}

TEST(ChannelTests, ChannelIsOpenUntilEndOfScope) {
    auto [sender, receiver] = create_channel<int>();
    EXPECT_TRUE(sender.is_open());
    EXPECT_TRUE(receiver.is_open());
    [s_ = std::move(sender)] {}();
    EXPECT_FALSE(sender.is_open());
    EXPECT_FALSE(receiver.is_open());
    [r_ = std::move(receiver)] {}();
    EXPECT_FALSE(sender.is_open());
    EXPECT_FALSE(receiver.is_open());
}

TEST(ChannelTests, SingleThread) {
    auto [sender, receiver] = create_channel<int>();
    sender.send(42);
    EXPECT_EQ(42, receiver.receive());
}

TEST(ChannelTests, TrySend) {
    auto [sender, receiver] = create_channel<int>();
    sender.send(42);
    EXPECT_EQ(receiver.receive(), 42);
    EXPECT_TRUE(sender.try_send(43));
    EXPECT_FALSE(sender.try_send(44));
}

TEST(ChannelTests, TryReceive) {
    auto [sender, receiver] = create_channel<int>();
    sender.send(42);
    auto result = receiver.try_receive();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
    result = receiver.try_receive();
    EXPECT_FALSE(result.has_value());
}

TEST(ChannelTests, BidirectionalChannels) {
    auto [a, b] = create_bidirectional_channel_pair<int>();
    a.send(42);
    EXPECT_EQ(42, b.receive());
    b.send(43);
    EXPECT_EQ(43, a.receive());
}

TEST(ChannelTests, BidirectionalChannelsSepeateThreads) {
    auto counter = std::atomic_int{};
    auto const do_work = [&counter](BidirectionalChannel<int> channel) {
        for (int i = 0; i < 100; ++i) {
            channel.send(i);
            EXPECT_EQ(channel.receive(), i);
            ++counter;
        }
    };
    {
        auto [a, b] = create_bidirectional_channel_pair<int>();
        auto threads = std::array{
            std::jthread{ do_work, std::move(a) },
            std::jthread{ do_work, std::move(b) },
        };
    }
    EXPECT_EQ(counter, 200);
    {
        auto [a, b] = create_bidirectional_channel_pair<int>();
        auto threads = std::array{
            std::jthread{ do_work, std::move(b) },
            std::jthread{ do_work, std::move(a) },
        };
    }
    EXPECT_EQ(counter, 400);
}
