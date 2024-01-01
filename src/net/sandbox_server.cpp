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
    //auto num_connections = std::size_t{ 0 };
    // clang-format off
    auto const socket = SocketLib::create_server_socket(
            AddressFamily::Unspecified,
            12345,
            [](ClientSocket const& client) -> void {
                //++num_connections;
                std::cerr << "server accepted a new client connection: " << client.os_socket_handle().value() << '\n';
                auto data = std::vector<std::byte>{};
                data.push_back(std::byte{'H'});
                data.push_back(std::byte{'e'});
                data.push_back(std::byte{'l'});
                data.push_back(std::byte{'l'});
                data.push_back(std::byte{'o'});
                data.push_back(std::byte{'\0'});
                //auto future = client.send(std::move(data));
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
    );
    // clang-format on

    std::this_thread::sleep_for(std::chrono::seconds(3));
    /*while (true) {
        std::cout << '.';
    }*/

#if 0
    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listen_socket);
        throw std::runtime_error{ std::format("failed to listen on socket: {}", WSAGetLastError()) };
    }

    auto const client_socket = accept(listen_socket, nullptr, nullptr);
    if (client_socket == INVALID_SOCKET) {
        closesocket(listen_socket);
        throw std::runtime_error{ std::format("accepting client socket failed: {}", WSAGetLastError()) };
    }

    while (true) {
        auto receive_buffer = std::array<char, 512>{};
        if (auto const num_bytes_received =
                    recv(client_socket, receive_buffer.data(), static_cast<int>(receive_buffer.size()), 0);
            num_bytes_received > 0) {
            std::cout << "bytes received: " << num_bytes_received << '\n';
            for (std::size_t i = 0; i < num_bytes_received; ++i) {
                std::cout << receive_buffer.at(i);
            }
            std::cout << '\n';

            if (auto const num_bytes_sent = send(client_socket, receive_buffer.data(), num_bytes_received, 0);
                num_bytes_sent == SOCKET_ERROR) {
                std::cerr << "echoing back to the client failed: " << WSAGetLastError() << '\n';
                closesocket(client_socket);
                break;
            } else {
                std::cout << "bytes sent: " << num_bytes_sent << '\n';
            }
        } else if (num_bytes_received == 0) {
            std::cout << "connection closing...\n";
            break;
        } else {
            std::cerr << "receiving data from client failed: " << WSAGetLastError() << '\n';
            break;
        }
    }
    closesocket(client_socket);
#endif
    std::cout << "successful program termination\n";
}
