#include "reliable_udp.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 3 || (std::string(argv[1]) != "server" &&
                     std::string(argv[1]) != "client")) {
        std::cerr << "Usage:\n"
                  << "  " << argv[0] << " server <port>\n"
                  << "  " << argv[0] << " client <host> <port>\n";
        return EXIT_FAILURE;
    }
    try {
        if (std::string(argv[1]) == "server") {
            const auto port = static_cast<std::uint16_t>(std::stoul(argv[2]));
            rudp::Endpoint endpoint("0.0.0.0", port, port);
            std::cout << "Listening on UDP port " << port << '\n';
            std::string message;
            while (endpoint.receive(message)) {
                std::cout << "received: " << message << '\n';
                endpoint.send("ack: " + message);
            }
        } else {
            if (argc < 4) return EXIT_FAILURE;
            const auto port = static_cast<std::uint16_t>(std::stoul(argv[3]));
            rudp::Endpoint endpoint(argv[2], port);
            std::string message;
            while (std::getline(std::cin, message)) {
                endpoint.send(message);
                if (endpoint.receive(message, 5000)) {
                    std::cout << "server: " << message << '\n';
                } else {
                    std::cerr << "timed out waiting for server response\n";
                }
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

