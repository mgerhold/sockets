#include <iostream>
#include <sockets/sockets.hpp>

static void run_sandbox_client() {
    using namespace c2k;
    auto socket = Sockets::create_client(AddressFamily::Unspecified, "localhost", 12345);
    std::cout << "connected to server at " << socket.remote_address() << '\n';
    auto buffer = MessageBuffer{};
    while (socket.is_connected()) {
        buffer << socket.receive(512).get();
        while (auto package = buffer.try_extract<int, int>()) {
            auto const [x, y] = package.value();
            std::cout << x << ',' << y << '\n';
        }
    }
}

int main() try { run_sandbox_client(); } catch (std::runtime_error const& e) {
    std::cerr << "execution terminated unexpectedly: " << e.what();
}
