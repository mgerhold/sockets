#include "net/sandbox_client.hpp"
#include "net/socket_lib.hpp"
#include <array>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string_view>

void run_sandbox_client() {
    auto socket = SocketLib::create_client_socket(AddressFamily::Unspecified, "localhost", 12345);

    auto text = std::string{};
    while (true) {
        auto const received = socket.receive(64).get();
        std::cerr << "received " << received.size() << " bytes\n";
        for (auto const byte : received) {
            if (static_cast<char>(byte) == '!') {
                goto after_loop;
            }
            text.push_back(static_cast<char>(byte));
        }
    }
after_loop:
    std::cerr << text.length() << '\n' << text << '\n';
    socket.send({ std::byte{ '!' } }).wait();
    std::cerr << "sent confirmation\n";
    std::cout << "successful program termination\n";
}
