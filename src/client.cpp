#include <iostream>
#include <net/sandbox_client.hpp>
#include <stdexcept>

int main() try { run_sandbox_client(); } catch (std::runtime_error const& e) {
    std::cerr << "execution terminated unexpectedly: " << e.what();
}
