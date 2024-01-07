#include <chrono>
#include <iomanip>
#include <iostream>
#include <sockets/sockets.hpp>
#include <sstream>

void run_sandbox_server() {
    using namespace std::chrono_literals;
    using namespace c2k;

    static constexpr auto accept_client = [](ClientSocket socket) {
        std::cout << "client connected from " << socket.remote_address() << '\n';
        auto thread = std::jthread{ [client_connection = std::move(socket)]() mutable {
            static constexpr auto repetitions = 30;
            for (auto i = 0; i < repetitions; ++i) {
                std::cout << "  sending \"" << i << ',' << i * 2 << "\" (" << (i + 1) << '/' << repetitions << ") to "
                          << client_connection.remote_address() << '\n';
                client_connection.send(i, i * 2).wait();
                if (i < repetitions - 1) {
                    std::this_thread::sleep_for(1s);
                }
            }
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
