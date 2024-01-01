#include <net/sandbox_server.hpp>
#include <net/socket_lib.hpp>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
// todo
#endif
#include <array>
#include <bit>
#include <cassert>
#include <iostream>
#include <stdexcept>

void run_sandbox_server() {
    auto received_confirmation = false;
    // clang-format off
    auto const socket = SocketLib::create_server_socket(
            AddressFamily::Unspecified,
            12345,
            [&](ClientSocket&& client) {
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
                received_confirmation = true;
            }
    );
    // clang-format on

    while (not received_confirmation) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::cerr << "ending program...\n";
    std::cout << "successful program termination\n";
}
