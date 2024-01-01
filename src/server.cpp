#include <iostream>
#include <net\sandbox_server.hpp>
#include <stdexcept>

int main() try { run_sandbox_server(); } catch (std::runtime_error const& e) {
    std::cerr << "execution terminated unexpectedly: " << e.what();
}
