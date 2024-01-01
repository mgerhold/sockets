#include <array>
#include <bit>
#include <cassert>
#include <condition_variable>
#include <iostream>
#include <net/sandbox_server.hpp>
#include <net/socket_lib.hpp>
#include <stdexcept>

void run_sandbox_server() {
    auto mutex = std::mutex{};
    auto received_confirmation = false;
    auto condition_variable = std::condition_variable{};
    // clang-format off
    auto const socket = SocketLib::create_server_socket(
            AddressFamily::Unspecified,
            12345,
            [&]([[maybe_unused]] ClientSocket client) {
                //++num_connections;
                std::cerr << "server accepted a new client connection: " << client.os_socket_handle().value() << '\n';
                auto data = std::vector<std::byte>{};
                data.push_back(std::byte{'H'});
                data.push_back(std::byte{'e'});
                data.push_back(std::byte{'l'});
                data.push_back(std::byte{'l'});
                data.push_back(std::byte{'o'});
                data.push_back(std::byte{'!'});
                auto future = client.send(std::move(data));
                std::this_thread::sleep_for(std::chrono::seconds(1));
                auto const num_bytes_sent = future.get();
                std::cerr << num_bytes_sent << " bytes sent\n";
                auto const received = client.receive(1).get();
                assert(received.size() == 1);
                std::cerr << "received confirmation: " << static_cast<char>(received.front()) << '\n';
                {
                    auto lock = std::scoped_lock{ mutex };
                    received_confirmation = true;
                }
                condition_variable.notify_one();
            }
    );
    // clang-format on

    auto lock = std::unique_lock{ mutex };
    condition_variable.wait(lock, [&] { return received_confirmation; });

    std::cerr << "ending program...\n";
    std::cout << "successful program termination\n";
}
