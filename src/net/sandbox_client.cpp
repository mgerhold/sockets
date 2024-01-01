#include "net/sandbox_client.hpp"
#include "net/socket_lib.hpp"

#include <array>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string_view>

void run_sandbox_client() {
    auto const socket = SocketLib::create_client_socket(AddressFamily::Unspecified, "localhost", 12345);

    std::this_thread::sleep_for(std::chrono::seconds(3));

#if 0
    using namespace std::string_view_literals;
    static constexpr auto to_send = "this is a test"sv;
    if (auto const num_bytes_sent = send(connect_socket, to_send.data(), static_cast<int>(to_send.length()), 0);
        num_bytes_sent == SOCKET_ERROR) {
        closesocket(connect_socket);
        throw std::runtime_error{ "unable to send data" };
    } else {
        std::cout << "bytes sent: " << num_bytes_sent << '\n';
    }

    if (shutdown(connect_socket, SD_SEND) == SOCKET_ERROR) {
        closesocket(connect_socket);
        throw std::runtime_error{ std::format("shutdown failed: {}", WSAGetLastError()) };
    }

    while (true) {
        auto receive_buffer = std::array<char, 512>{};
        auto const num_bytes_received =
                recv(connect_socket, receive_buffer.data(), static_cast<int>(receive_buffer.size()), 0);
        if (num_bytes_received > 0) {
            assert(num_bytes_received <= receive_buffer.size());
            std::cout << "bytes received: " << num_bytes_received << '\n';
            for (std::size_t i = 0; i < num_bytes_received; ++i) {
                std::cout << receive_buffer.at(i);
            }
            std::cout << '\n';
        } else if (num_bytes_received == 0) {
            std::cout << "connection closed\n";
            break;
        } else {
            std::cerr << "receive failed: " << WSAGetLastError() << '\n';
            break;
        }
    };

    closesocket(connect_socket);
    std::cout << "socket closed\n";

#endif
    std::cout << "successful program termination\n";
}
