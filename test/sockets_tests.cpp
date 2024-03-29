#include <future>
#include <gtest/gtest.h>
#include <numeric>
#include <sockets/sockets.hpp>

static constexpr auto localhost = "127.0.0.1";

[[nodiscard]] static std::vector<std::byte> iota(std::size_t const count, std::byte value = std::byte{ 0 }) {
    auto data = std::vector<std::byte>{};
    data.reserve(count);
    for (auto i = std::size_t{ 0 }; i < count; ++i) {
        data.push_back(value);
        value = static_cast<std::byte>(static_cast<std::uint8_t>(value) + 1); // unsigned overflow wraparound
    }
    return data;
}

TEST(SocketsTests, SendAndReceive) {
    auto promise = std::promise<char>{};
    auto future = promise.get_future();
    auto const server = c2k::Sockets::create_server(c2k::AddressFamily::Ipv4, 0, [&promise](c2k::ClientSocket client) {
        auto message_buffer = c2k::MessageBuffer{};
        message_buffer << client.receive(1).get();
        promise.set_value(message_buffer.try_extract<char>().value());
    });

    auto const port = server.local_address().port;

    static constexpr auto value = 'A';
    auto client = c2k::Sockets::create_client(c2k::AddressFamily::Ipv4, localhost, port);
    auto const num_bytes_sent = client.send(value).get();
    EXPECT_EQ(num_bytes_sent, sizeof(value));
    EXPECT_EQ(future.get(), value);
}

TEST(SocketsTests, ReceiveExact) {
    auto promise = std::promise<int>{};
    auto future = promise.get_future();
    auto const server = c2k::Sockets::create_server(c2k::AddressFamily::Ipv4, 0, [&promise](c2k::ClientSocket client) {
        auto buffer = c2k::MessageBuffer{};
        buffer << client.receive_exact(sizeof(int)).get();
        promise.set_value(buffer.try_extract<int>().value());
    });

    auto const port = server.local_address().port;

    static constexpr auto value = 42;
    auto client = c2k::Sockets::create_client(c2k::AddressFamily::Ipv4, localhost, port);
    auto const num_bytes_sent = client.send(value).get();
    EXPECT_EQ(num_bytes_sent, sizeof(value));
    EXPECT_EQ(future.get(), value);
}

TEST(SocketsTests, ReceiveExactManyBytes) {
    static constexpr auto size = std::size_t{ 1024 * 1024 };
    static constexpr auto num_chunks = std::size_t{ 16 };
    static constexpr auto chunk_size = size / num_chunks;

    auto promise = std::promise<std::vector<std::byte>>{};
    auto future = promise.get_future();
    auto const server = c2k::Sockets::create_server(c2k::AddressFamily::Ipv4, 0, [&promise](c2k::ClientSocket client) {
        promise.set_value(client.receive_exact(size).get());

        using namespace std::chrono_literals;
        std::this_thread::sleep_for(200ms); // keep connection open a bit longer
    });

    auto const port = server.local_address().port;

    auto const data = iota(size);

    auto client = c2k::Sockets::create_client(c2k::AddressFamily::Ipv4, localhost, port);

    for (auto i = std::size_t{ 0 }; i < num_chunks; ++i) {
        auto chunk = std::vector<std::byte>{};
        using Difference = decltype(chunk)::difference_type;
        chunk.resize(chunk_size);
        // clang-format off
        std::copy(
            data.cbegin() + static_cast<Difference>(i * chunk_size),
            data.cbegin() + static_cast<Difference>((i + 1) * chunk_size),
            chunk.begin()
        );
        // clang-format on
        auto const num_bytes_sent = client.send(chunk).get();
        EXPECT_EQ(num_bytes_sent, chunk_size);
    }

    EXPECT_EQ(future.get(), data);
}

TEST(SocketsTests, ReceiveExactManyBytesWithTimeout) {
    using namespace std::chrono_literals;

    static constexpr auto size = std::size_t{ 1024 * 1024 };
    static constexpr auto num_chunks = std::size_t{ 16 };
    static constexpr auto chunk_size = size / num_chunks;

    auto promise = std::promise<std::vector<std::byte>>{};
    auto future = promise.get_future();
    auto const server = c2k::Sockets::create_server(c2k::AddressFamily::Ipv4, 0, [&promise](c2k::ClientSocket client) {
        promise.set_value(client.receive_exact(size, 1s).get());

        using namespace std::chrono_literals;
        std::this_thread::sleep_for(200ms); // keep connection open a bit longer
    });

    auto const port = server.local_address().port;

    auto const data = iota(size);

    auto client = c2k::Sockets::create_client(c2k::AddressFamily::Ipv4, localhost, port);

    for (auto i = std::size_t{ 0 }; i < num_chunks; ++i) {
        auto chunk = std::vector<std::byte>{};
        using Difference = decltype(chunk)::difference_type;
        chunk.resize(chunk_size);
        // clang-format off
        std::copy(
            data.cbegin() + static_cast<Difference>(i * chunk_size),
            data.cbegin() + static_cast<Difference>((i + 1) * chunk_size),
            chunk.begin()
        );
        // clang-format on
        auto const num_bytes_sent = client.send(chunk).get();
        EXPECT_EQ(num_bytes_sent, chunk_size);
    }

    EXPECT_EQ(future.get(), data);
}

TEST(SocketsTests, ReceiveExactManyBytesWithExceededTimeoutThrowsException) {
    using namespace std::chrono_literals;

    static constexpr auto size = std::size_t{ 1024 * 1024 };
    static constexpr auto num_chunks = std::size_t{ 4 };
    static constexpr auto chunk_size = size / num_chunks;

    auto promise = std::promise<c2k::ClientSocket>{};
    auto future = promise.get_future();
    auto const server = c2k::Sockets::create_server(c2k::AddressFamily::Ipv4, 0, [&promise](c2k::ClientSocket client) {
        promise.set_value(std::move(client));
    });

    auto const port = server.local_address().port;

    auto const data = iota(size);

    auto thread = std::jthread{ [&data, port] {
        auto client = c2k::Sockets::create_client(c2k::AddressFamily::Ipv4, localhost, port);

        for (auto i = std::size_t{ 0 }; i < num_chunks; ++i) {
            auto chunk = std::vector<std::byte>{};
            using Difference = decltype(chunk)::difference_type;
            chunk.resize(chunk_size);
            // clang-format off
            std::copy(
                data.cbegin() + static_cast<Difference>(i * chunk_size),
                data.cbegin() + static_cast<Difference>((i + 1) * chunk_size),
                chunk.begin()
            );
            // clang-format on
            client.send(chunk).wait();
            std::this_thread::sleep_for(100ms);
        }
    } };

    auto client_connection = future.get();

    EXPECT_THROW({ std::ignore = client_connection.receive_exact(size, 100ms).get(); }, c2k::TimeoutError);
}

TEST(SocketsTests, ReceiveWithExceededTimeoutThrowsException) {
    using namespace std::chrono_literals;

    auto promise = std::promise<c2k::ClientSocket>{};
    auto future = promise.get_future();
    auto const server = c2k::Sockets::create_server(c2k::AddressFamily::Ipv4, 0, [&promise](c2k::ClientSocket client) {
        promise.set_value(std::move(client));
    });

    auto const port = server.local_address().port;

    auto client = c2k::Sockets::create_client(c2k::AddressFamily::Ipv4, localhost, port);
    auto client_connection = future.get();

    EXPECT_EQ(client_connection.receive(1, 100ms).get(), std::vector<std::byte>{});
}

TEST(SocketsTests, ReceiveExactWithoutTimeoutWillTimeoutIfNoDataCanBeRead) {
    auto promise = std::promise<c2k::ClientSocket>{};
    auto future = promise.get_future();
    auto const server = c2k::Sockets::create_server(c2k::AddressFamily::Ipv4, 0, [&promise](c2k::ClientSocket client) {
        promise.set_value(std::move(client));
    });

    auto const port = server.local_address().port;

    auto client = c2k::Sockets::create_client(c2k::AddressFamily::Ipv4, localhost, port);
    auto client_connection = future.get();

    EXPECT_THROW(client_connection.receive_exact(1).get(), c2k::TimeoutError);
}

TEST(SocketsTests, ReceiveWithoutTimeoutWillReturnEmptyVector) {
    auto promise = std::promise<c2k::ClientSocket>{};
    auto future = promise.get_future();
    auto const server = c2k::Sockets::create_server(c2k::AddressFamily::Ipv4, 0, [&promise](c2k::ClientSocket client) {
        promise.set_value(std::move(client));
    });

    auto const port = server.local_address().port;

    auto client = c2k::Sockets::create_client(c2k::AddressFamily::Ipv4, localhost, port);
    auto client_connection = future.get();

    EXPECT_EQ(client_connection.receive(1).get(), std::vector<std::byte>{});
}

TEST(SocketsTests, ReceiveExactMultipleTimes) {
    static constexpr auto chunk_size = std::size_t{ 128 };
    static constexpr auto num_chunks = std::size_t{ 4 };
    static constexpr auto size = std::size_t{ chunk_size * num_chunks };

    auto const data = iota(size);

    auto promise = std::promise<c2k::ClientSocket>{};
    auto future = promise.get_future();
    auto const server = c2k::Sockets::create_server(c2k::AddressFamily::Ipv4, 0, [&promise](c2k::ClientSocket client) {
        promise.set_value(std::move(client));
    });

    auto const port = server.local_address().port;

    auto client = c2k::Sockets::create_client(c2k::AddressFamily::Ipv4, localhost, port);

    EXPECT_EQ(client.send(data).get(), size);

    auto client_connection = future.get();
    for (auto i = std::size_t{ 0 }; i < num_chunks; ++i) {
        using Difference = decltype(data)::difference_type;
        auto expected_chunk = std::vector<std::byte>{};
        expected_chunk.reserve(chunk_size);
        std::copy(
                data.cbegin() + static_cast<Difference>(i * chunk_size),
                data.cbegin() + static_cast<Difference>((i + 1) * chunk_size),
                std::back_inserter(expected_chunk)
        );
        auto const actual_chunk = client_connection.receive_exact(chunk_size).get();
        EXPECT_EQ(expected_chunk, actual_chunk);
    }
}


TEST(SocketsTests, ServerInitialization) {
    auto const server =
            c2k::Sockets::create_server(c2k::AddressFamily::Ipv4, 0, []([[maybe_unused]] c2k::ClientSocket client) {});
    EXPECT_TRUE(server.local_address().port != 0);
}

TEST(SocketsTests, ClientInitialization) {
    auto const server = c2k::Sockets::create_server(c2k::AddressFamily::Ipv4, 0, [](auto) {});

    auto const port = server.local_address().port;

    auto const client = c2k::Sockets::create_client(c2k::AddressFamily::Ipv4, localhost, port);
    EXPECT_EQ(client.remote_address().port, port);
}

TEST(SocketsTests, SendAndReceiveMultipleTimes) {
    using namespace std::chrono_literals;
    auto promise = std::promise<std::vector<char>>{};
    auto future = promise.get_future();
    auto const server = c2k::Sockets::create_server(c2k::AddressFamily::Ipv4, 0, [&promise](c2k::ClientSocket client) {
        auto result = std::vector<char>{};
        for (auto i = 0; i < 5; i++) {
            auto buffer = c2k::MessageBuffer{};
            buffer << client.receive(1).get();
            result.push_back(buffer.try_extract<char>().value());
        }
        promise.set_value(result);
        std::this_thread::sleep_for(500ms); // keep client socket open for a bit longer
    });

    auto const port = server.local_address().port;
    static constexpr auto value = 'B';
    auto client = c2k::Sockets::create_client(c2k::AddressFamily::Ipv4, localhost, port);
    for (auto i = 0; i < 5; i++) {
        auto const num_bytes_sent = client.send(value).get();
        EXPECT_EQ(num_bytes_sent, sizeof(value));
    }

    for (auto const& c : future.get()) {
        EXPECT_EQ(c, value);
    }
}

TEST(SocketsTests, ReceiveIntegralValues) {
    static constexpr auto values = std::tuple{ 124234, 97234L, 'a', true, short{ 13 }, std::uint64_t{ 1356469817 } };
    auto server = c2k::Sockets::create_server(c2k::AddressFamily::Ipv4, 0, [](c2k::ClientSocket client) {
        auto const num_bytes_sent = client.send(std::get<0>(values),
                                                std::get<1>(values),
                                                std::get<2>(values),
                                                std::get<3>(values),
                                                std::get<4>(values),
                                                std::get<5>(values))
                                            .get();
        EXPECT_EQ(
                num_bytes_sent,
                sizeof(decltype(std::get<0>(values))) + sizeof(decltype(std::get<1>(values)))
                        + sizeof(decltype(std::get<2>(values))) + sizeof(decltype(std::get<3>(values)))
                        + sizeof(decltype(std::get<4>(values))) + sizeof(decltype(std::get<5>(values)))
        );
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(100ms);
    });

    auto const port = server.local_address().port;

    auto client = c2k::Sockets::create_client(c2k::AddressFamily::Ipv4, localhost, port);
    auto const received = client.receive<
                                        std::remove_cvref_t<decltype(std::get<0>(values))>,
                                        std::remove_cvref_t<decltype(std::get<1>(values))>,
                                        std::remove_cvref_t<decltype(std::get<2>(values))>,
                                        std::remove_cvref_t<decltype(std::get<3>(values))>,
                                        std::remove_cvref_t<decltype(std::get<4>(values))>,
                                        std::remove_cvref_t<decltype(std::get<5>(values))>>()
                                  .get();
    EXPECT_EQ(received, values);
}

TEST(SocketsTests, ReceiveIntegralValuesExceedingDefaultTimeoutThrowsException) {
    static constexpr auto values = std::tuple{ 124234, 97234L, 'a', true, short{ 13 }, std::uint64_t{ 1356469817 } };
    auto server = c2k::Sockets::create_server(c2k::AddressFamily::Ipv4, 0, [](c2k::ClientSocket client) {
        using namespace std::chrono_literals;

        auto const num_bytes_sent = client.send(std::get<0>(values), std::get<1>(values), std::get<2>(values)).get();
        EXPECT_EQ(
                num_bytes_sent,
                sizeof(decltype(std::get<0>(values))) + sizeof(decltype(std::get<1>(values)))
                        + sizeof(decltype(std::get<2>(values)))
        );

        std::this_thread::sleep_for(1200ms);
    });

    auto const port = server.local_address().port;

    auto client = c2k::Sockets::create_client(c2k::AddressFamily::Ipv4, localhost, port);

    auto const code_that_should_throw = [&client] {
        [[maybe_unused]] auto const received = client.receive<
                                                             std::remove_cvref_t<decltype(std::get<0>(values))>,
                                                             std::remove_cvref_t<decltype(std::get<1>(values))>,
                                                             std::remove_cvref_t<decltype(std::get<2>(values))>,
                                                             std::remove_cvref_t<decltype(std::get<3>(values))>,
                                                             std::remove_cvref_t<decltype(std::get<4>(values))>,
                                                             std::remove_cvref_t<decltype(std::get<5>(values))>>()
                                                       .get();
    };

    EXPECT_THROW({ code_that_should_throw(); }, c2k::TimeoutError);
}

struct Vector2 {
    double x;
    double y;
};

TEST(SocketsTests, ReceiveIntegralValuesExceedingCustomTimeoutThrowsException) {
    static constexpr auto values = std::tuple{ 124234, 97234L, 'a', true, short{ 13 }, std::uint64_t{ 1356469817 } };
    using namespace std::chrono_literals;

    auto server = c2k::Sockets::create_server(c2k::AddressFamily::Ipv4, 0, [](c2k::ClientSocket client) {
        auto const num_bytes_sent = client.send(std::get<0>(values), std::get<1>(values), std::get<2>(values)).get();
        EXPECT_EQ(
                num_bytes_sent,
                sizeof(decltype(std::get<0>(values))) + sizeof(decltype(std::get<1>(values)))
                        + sizeof(decltype(std::get<2>(values)))
        );
        std::this_thread::sleep_for(300ms);
    });

    auto const port = server.local_address().port;

    auto client = c2k::Sockets::create_client(c2k::AddressFamily::Ipv4, localhost, port);

    auto const code_that_should_throw = [&client] {
        [[maybe_unused]] auto const received = client.receive<
                                                             std::remove_cvref_t<decltype(std::get<0>(values))>,
                                                             std::remove_cvref_t<decltype(std::get<1>(values))>,
                                                             std::remove_cvref_t<decltype(std::get<2>(values))>,
                                                             std::remove_cvref_t<decltype(std::get<3>(values))>,
                                                             std::remove_cvref_t<decltype(std::get<4>(values))>,
                                                             std::remove_cvref_t<decltype(std::get<5>(values))>>(200ms)
                                                       .get();
    };

    EXPECT_THROW({ code_that_should_throw(); }, c2k::TimeoutError);
}