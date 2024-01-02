#include <iostream>
#include <sockets/socket_lib.hpp>

namespace c2k {
    void run_sandbox_client() {
        auto socket = SocketLib::create_client_socket(AddressFamily::Unspecified, "localhost", 12345);

        auto buffer = std::string{};
        while (socket.is_connected()) {
            buffer += socket.receive_string(512).get();
            auto const newline_position = buffer.find('\n');
            if (newline_position != std::string::npos) {
                std::cout << buffer.substr(0, newline_position) << '\n'; // contains newline
                buffer.erase(0, newline_position + 1);
            }
        }
    }
} // namespace c2k

int main() try { c2k::run_sandbox_client(); } catch (std::runtime_error const& e) {
    std::cerr << "execution terminated unexpectedly: " << e.what();
}
