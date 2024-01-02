#include <chrono>
#include <iomanip>
#include <iostream>
#include <sockets/socket_lib.hpp>
#include <sstream>

void run_sandbox_server() {
    using namespace std::chrono_literals;
    using namespace c2k;

    static constexpr auto port = std::uint16_t{ 12345 };

    auto const server = SocketLib::create_server_socket(AddressFamily::Unspecified, port, [&](ClientSocket socket) {
        std::cout << "client connected\n";
        auto thread = std::jthread{ [client_connection = std::move(socket)]() mutable {
            static constexpr auto repetitions = 5;
            for (int i = 0; i < repetitions; ++i) {
                auto const text = std::to_string(i);
                std::cout << "  sending \"" << text << "\" (" << (i + 1) << '/' << repetitions << ")...\n";
                client_connection.send(text + '\n').wait();
                if (i < repetitions - 1) {
                    std::this_thread::sleep_for(1s);
                }
            }
            client_connection.send("thank you for travelling with Deutsche Bahn\n").wait();
            std::cout << "  farewell, little client!\n";
        } };
        thread.detach();
    });
    std::cout << "listening on port " << port << "...\n";

    // sleep forever to keep server alive
    std::promise<void>{}.get_future().wait();
}

int main() try { run_sandbox_server(); } catch (std::runtime_error const& e) {
    std::cerr << "execution terminated unexpectedly: " << e.what();
}
