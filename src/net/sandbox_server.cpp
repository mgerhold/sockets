#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "net/sandbox_server.hpp"
#include "net/socket_lib.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

void run_sandbox_server() {
    using namespace std::chrono_literals;

    static constexpr auto port = std::uint16_t{ 12345 };

    auto const server = SocketLib::create_server_socket(AddressFamily::Unspecified, port, [&](ClientSocket socket) {
        std::cout << "client connected\n";
        auto thread = std::jthread{ [client_connection = std::move(socket)]() mutable {
            static constexpr auto repetitions = 5;
            for (int i = 0; i < repetitions; ++i) {
                auto const text = std::to_string(i);
                std::cout << "  sending \"" << text << "\" (" << (i + 1) << '/' << repetitions << ")..." << std::endl;
                client_connection.send(text + '\n').wait();
                if (i < repetitions - 1) {
                    std::this_thread::sleep_for(1s);
                }
            }
            client_connection.send("thank you for travelling with Deutsche Bahn\n").wait();
            std::cout << "  farewell, little client!" << std::endl;
        } };
        thread.detach();
    });
    std::cout << "listening on port " << port << "..." << std::endl;

    // sleep forever to keep server alive
    std::promise<void>{}.get_future().wait();
}
