#include <array>
#include <functional>
#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <numeric>
#include <sockets/sockets.hpp>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

using namespace c2k;

TEST(Synchronized, CreateInstances) {
    [[maybe_unused]] auto s1 = Synchronized{ 42 };
    [[maybe_unused]] auto s2 = Synchronized{ std::string{ "this is a test string for testing purposes" } };
    [[maybe_unused]] auto s3 =
            Synchronized{ std::make_unique<std::string>("this is a test string for testing purposes") };
}

TEST(Synchronized, LockModifyRead) {
    auto s = Synchronized{ 42 };
    auto const first = s.apply(std::identity{});
    EXPECT_EQ(first, 42);
    s.apply([](int& i) { ++i; });
    auto const second = s.apply(std::identity{});
    EXPECT_EQ(second, 43);
}

TEST(Synchronized, AccessFromDifferentThreads) {
    static constexpr auto num_threads = std::size_t{ 2 };
    auto numbers = std::vector<std::size_t>{};
    auto numbers_mutex = std::mutex{};
    auto keep_increasing =
            [&](std::stop_token const& stop_token, std::size_t& loop_counter, Synchronized<std::size_t>& counter) {
                while (not stop_token.stop_requested()) {
                    counter.apply([&](std::size_t& value) {
                        auto lock = std::scoped_lock{ numbers_mutex };
                        numbers.push_back(value++);
                    });
                    ++loop_counter;
                }
            };
    auto loop_counters = std::array<std::size_t, num_threads>{};
    auto threads = std::vector<std::jthread>{};
    threads.reserve(num_threads);
    auto synchronized = Synchronized{ std::size_t{} };
    for (auto i = std::size_t{ 0 }; i < num_threads; ++i) {
        threads.emplace_back(keep_increasing, std::ref(loop_counters.at(i)), std::ref(synchronized));
    }

    std::this_thread::sleep_for(std::chrono::seconds{ 10 });

    for (auto& thread : threads) {
        thread.request_stop();
        thread.join();
    }

    synchronized.apply([&](auto const value) {
        EXPECT_EQ(value, numbers.size());
        for (auto i = std::size_t{ 0 }; i < value; ++i) {
            EXPECT_EQ(i, numbers.at(i));
        }
    });
    EXPECT_EQ(std::accumulate(loop_counters.cbegin(), loop_counters.cend(), std::size_t{ 0 }), numbers.size());
}

TEST(Synchronized, TryLockTwiceFromSameThread) {
    auto synchronized = Synchronized{ 42 };
    synchronized.apply([&](int& i) {
        EXPECT_EQ(i, 42);
        ++i;
        EXPECT_EQ(i, 43);
        synchronized.apply([&](int& j) {
            EXPECT_EQ(i, 43);
            EXPECT_EQ(j, 43);
            ++i;
            EXPECT_EQ(i, 44);
            EXPECT_EQ(j, 44);
            ++j;
            EXPECT_EQ(i, 45);
            EXPECT_EQ(j, 45);
        });
        EXPECT_EQ(i, 45);
    });
    EXPECT_EQ(synchronized.apply(std::identity{}), 45);
}
