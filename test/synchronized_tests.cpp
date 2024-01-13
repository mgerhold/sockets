#include <array>
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

TEST(Synchronized, AccessFromDifferentThreads) {
    static constexpr auto num_threads = std::size_t{ 2 };
    auto numbers = std::vector<std::size_t>{};
    auto numbers_mutex = std::mutex{};
    auto keep_increasing =
            [&](std::stop_token const& stop_token, std::size_t& loop_counter, Synchronized<std::size_t>& counter) {
                while (not stop_token.stop_requested()) {
                    {
                        auto locked = counter.lock();
                        auto lock = std::scoped_lock{ numbers_mutex };
                        numbers.push_back((*locked)++);
                    }
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

    auto locked = synchronized.lock();
    EXPECT_EQ(*locked, numbers.size());
    for (auto i = std::size_t{ 0 }; i < *locked; ++i) {
        EXPECT_EQ(i, numbers.at(i));
    }
    static constexpr auto expected_fraction = 1.0 / static_cast<double>(num_threads);
    static constexpr auto max_relative_deviation = 0.2;
    static constexpr auto expected_min_fraction = expected_fraction * (1.0 - max_relative_deviation);
    static constexpr auto expected_max_fraction = expected_fraction * (1.0 + max_relative_deviation);
    for (auto const& loop_counter : loop_counters) {
        EXPECT_GT(loop_counter, 0);
        auto const fraction = static_cast<double>(loop_counter) / static_cast<double>(numbers.size());
        // the following two expects are a bit "fishy" since they depend on the OS's scheduler, but we still
        // keep them in
        EXPECT_GE(fraction, expected_min_fraction);
        EXPECT_LE(fraction, expected_max_fraction);
    }
    EXPECT_EQ(std::accumulate(loop_counters.cbegin(), loop_counters.cend(), std::size_t{ 0 }), numbers.size());
}

// TEST(Synchronized, DanglingReferencesTerminate) {
//     ASSERT_DEATH(
//             {
//                 auto const locked = [] {
//                     auto synchronized = Synchronized{ 42 };
//                     return synchronized.lock();
//                 }();
//             },
//             "cannot destroy Synchronized object while there's still an active lock"
//     );
// }
