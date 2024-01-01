#include "net/sandbox_client.hpp"
#include "net/socket_lib.hpp"
#include <iostream>
#include <string_view>

void run_sandbox_client() {
    auto socket = SocketLib::create_client_socket(AddressFamily::Unspecified, "localhost", 12345);

    auto num_data_points = 0;
    auto buffer = std::string{};
    while (true) {
        buffer += socket.receive_string(512).get();
        auto const newline_position = buffer.find('\n');
        if (newline_position != std::string::npos) {
            std::cout << buffer.substr(0, newline_position) << std::endl; // contains newline
            buffer.erase(0, newline_position + 1);
            ++num_data_points;
            if (num_data_points >= 5) {
                break;
            }
        }
    }
}
