#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "net/sandbox_server.hpp"
#include "net/socket_lib.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

[[nodiscard]] std::string current_date_time() {
    auto const now = std::chrono::system_clock::now();
    auto const now_c = std::chrono::system_clock::to_time_t(now);
    auto ss = std::stringstream{};
    ss << std::put_time(std::localtime(&now_c), "%F %T");
    return ss.str();
}

void run_sandbox_server() {
    using namespace std::chrono_literals;

    static constexpr auto port = std::uint16_t{ 12345 };

    auto const socket = SocketLib::create_server_socket(
            AddressFamily::Unspecified,
            port,
            [&]([[maybe_unused]] ClientSocket client_connection) {
                std::cout << "client connected\n";
                while (client_connection.is_connected()) {
                    auto const text = current_date_time();
                    std::cout << "  sending \"" << text << "\"..." << std::endl;
                    client_connection.send(text + '\n').wait();
                    std::this_thread::sleep_for(1s);
                }
            }
    );
    std::cout << "listening on port " << port << "..." << std::endl;

    // sleep forever to keep server alive
    std::promise<void>{}.get_future().wait();
}
