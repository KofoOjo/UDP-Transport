#include "reliable_udp.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

std::string json_escape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default: escaped += character; break;
        }
    }
    return escaped;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <host> <port>\n";
        return EXIT_FAILURE;
    }

    try {
        const auto port = static_cast<std::uint16_t>(std::stoul(argv[2]));
        rudp::Endpoint endpoint(argv[1], port);
        std::string message;
        while (std::getline(std::cin, message)) {
            endpoint.send(message);
            std::string response;
            if (endpoint.receive(response, 5000)) {
                std::cout << "{\"ok\":true,\"message\":\""
                          << json_escape(response) << "\"}\n";
            } else {
                std::cout << "{\"ok\":false,\"error\":\"UDP response timed out\"}\n";
            }
            std::cout.flush();
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
