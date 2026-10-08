#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rudp {

class Endpoint {
public:
    // Connects a UDP socket to peer_host:peer_port. The socket is still
    // connectionless at the network level, but the kernel filters stray peers.
    Endpoint(const std::string& peer_host, std::uint16_t peer_port,
             std::uint16_t local_port = 0);
    ~Endpoint();

    Endpoint(const Endpoint&) = delete;
    Endpoint& operator=(const Endpoint&) = delete;

    // Reliably sends one application message. Messages larger than the
    // protocol payload are split into packets and reassembled by the peer.
    void send(const std::string& message);

    // Waits for one complete application message. Returns false on timeout.
    bool receive(std::string& message, int timeout_ms = -1);

private:
    struct Impl;
    Impl* impl_;
};

} // namespace rudp

