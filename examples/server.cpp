#include <chrono>
#include <iomanip>
#include <iostream>
#include <sockets/sockets.hpp>
#include <sstream>

void run_sandbox_server() {
    using namespace std::chrono_literals;
    using namespace c2k;

    static constexpr auto accept_client = [](ClientSocket socket) {
        // TODO: print that a client connected and include the protocol in the message
        auto thread = std::jthread{ [client_connection = std::move(socket)]() mutable {
            static constexpr auto repetitions = 30;
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
    };

    static constexpr auto port = std::uint16_t{ 12345 };

    auto const ipv4_server = Sockets::create_server(AddressFamily::Ipv4, port, accept_client);
    auto const ipv6_server = Sockets::create_server(AddressFamily::Ipv6, port, accept_client);
    std::cout << "listening on port " << port << " on IPv4 and IPv6...\n";

    // sleep forever to keep server alive
    std::promise<void>{}.get_future().wait();
}

// clang-format off
int main() try {
    run_sandbox_server();
} catch (std::runtime_error const& e) {
    std::cerr << "execution terminated unexpectedly: " << e.what();
}
// clang-format on
