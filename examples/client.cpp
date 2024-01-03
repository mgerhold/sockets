#include <iostream>
#include <sockets/sockets.hpp>

static void run_sandbox_client() {
    using namespace c2k;
    auto socket = Sockets::create_client(AddressFamily::Unspecified, "localhost", 12345);

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

int main() try { run_sandbox_client(); } catch (std::runtime_error const& e) {
    std::cerr << "execution terminated unexpectedly: " << e.what();
}
