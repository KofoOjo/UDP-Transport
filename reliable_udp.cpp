#include "reliable_udp.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace rudp {
namespace {

constexpr std::uint16_t kMagic = 0x5255;
constexpr std::uint8_t kVersion = 1;
constexpr std::size_t kHeaderSize = 22;
constexpr std::size_t kMaxDatagram = 1200;
constexpr std::size_t kMaxPayload = kMaxDatagram - kHeaderSize;
constexpr std::uint16_t kReceiveWindow = 64;
constexpr auto kRetransmitTimeout = std::chrono::milliseconds(120);

enum class Type : std::uint8_t { Data = 1, Ack = 2 };

struct Packet {
    Type type;
    std::uint32_t sequence;
    std::uint32_t acknowledgement;
    std::uint16_t window;
    std::vector<std::uint8_t> payload;
};

void put16(std::uint8_t* p, std::uint16_t value) {
    p[0] = static_cast<std::uint8_t>(value >> 8);
    p[1] = static_cast<std::uint8_t>(value);
}

void put32(std::uint8_t* p, std::uint32_t value) {
    for (int i = 3; i >= 0; --i) {
        p[3 - i] = static_cast<std::uint8_t>(value >> (i * 8));
    }
}

std::uint16_t get16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>((p[0] << 8) | p[1]);
}

std::uint32_t get32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) |
           p[3];
}

std::uint32_t checksum(const std::uint8_t* data, std::size_t size) {
    std::uint32_t hash = 2166136261u;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

std::vector<std::uint8_t> encode(const Packet& packet) {
    if (packet.payload.size() > kMaxPayload) {
        throw std::invalid_argument("message fragment exceeds maximum payload");
    }
    std::vector<std::uint8_t> bytes(kHeaderSize + packet.payload.size(), 0);
    put16(bytes.data(), kMagic);
    bytes[2] = kVersion;
    bytes[3] = static_cast<std::uint8_t>(packet.type);
    put32(bytes.data() + 6, packet.sequence);
    put32(bytes.data() + 10, packet.acknowledgement);
    put16(bytes.data() + 14, packet.window);
    put16(bytes.data() + 16, static_cast<std::uint16_t>(packet.payload.size()));
    std::copy(packet.payload.begin(), packet.payload.end(), bytes.begin() + kHeaderSize);
    put32(bytes.data() + 18, checksum(bytes.data(), 18));
    if (!packet.payload.empty()) {
        // The checksum covers the complete datagram, with its checksum field
        // excluded from the hash.
        std::uint32_t hash = checksum(bytes.data(), 18);
        for (std::size_t i = kHeaderSize; i < bytes.size(); ++i) {
            hash ^= bytes[i];
            hash *= 16777619u;
        }
        put32(bytes.data() + 18, hash);
    }
    return bytes;
}

bool decode(const std::uint8_t* bytes, std::size_t size, Packet& packet) {
    if (size < kHeaderSize || get16(bytes) != kMagic || bytes[2] != kVersion) {
        return false;
    }
    const auto payload_size = get16(bytes + 16);
    if (payload_size != size - kHeaderSize || payload_size > kMaxPayload) {
        return false;
    }
    auto hash = checksum(bytes, 18);
    for (std::size_t i = kHeaderSize; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    if (hash != get32(bytes + 18)) {
        return false;
    }
    if (bytes[3] != static_cast<std::uint8_t>(Type::Data) &&
        bytes[3] != static_cast<std::uint8_t>(Type::Ack)) {
        return false;
    }
    packet.type = static_cast<Type>(bytes[3]);
    packet.sequence = get32(bytes + 6);
    packet.acknowledgement = get32(bytes + 10);
    packet.window = get16(bytes + 14);
    packet.payload.assign(bytes + kHeaderSize, bytes + size);
    return true;
}

int make_socket(const std::string& host, std::uint16_t port,
                std::uint16_t local_port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_NUMERICHOST;
    addrinfo* result = nullptr;
    const auto port_string = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_string.c_str(), &hints, &result) != 0) {
        hints.ai_flags = 0;
        if (getaddrinfo(host.c_str(), port_string.c_str(), &hints, &result) != 0) {
            throw std::runtime_error("unable to resolve peer address");
        }
    }
    const int fd = socket(result->ai_family, SOCK_DGRAM, 0);
    if (fd < 0) {
        freeaddrinfo(result);
        throw std::runtime_error("unable to create UDP socket");
    }
    if (local_port != 0) {
        sockaddr_storage local{};
        if (result->ai_family == AF_INET) {
            auto* address = reinterpret_cast<sockaddr_in*>(&local);
            address->sin_family = AF_INET;
            address->sin_addr.s_addr = htonl(INADDR_ANY);
            address->sin_port = htons(local_port);
        } else {
            auto* address = reinterpret_cast<sockaddr_in6*>(&local);
            address->sin6_family = AF_INET6;
            address->sin6_port = htons(local_port);
        }
        if (bind(fd, reinterpret_cast<sockaddr*>(&local),
                 result->ai_addrlen) < 0) {
            close(fd);
            freeaddrinfo(result);
            throw std::runtime_error("unable to bind local UDP port");
        }
    }
    if (host != "0.0.0.0") {
        if (connect(fd, result->ai_addr, result->ai_addrlen) < 0) {
            close(fd);
            freeaddrinfo(result);
            throw std::runtime_error("unable to connect UDP peer");
        }
    }
    freeaddrinfo(result);
    timeval timeout{0, 100000};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    return fd;
}

} // namespace

struct Endpoint::Impl {
    explicit Impl(const std::string& host, std::uint16_t port,
                  std::uint16_t local_port)
        : socket_fd(make_socket(host, port, local_port)),
          connected(host != "0.0.0.0"),
          worker(&Impl::read_loop, this) {}

    ~Impl() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping = true;
        }
        worker.join();
        close(socket_fd);
    }

    void transmit(const Packet& packet) {
        const auto bytes = encode(packet);
        if (::send(socket_fd, bytes.data(), bytes.size(), 0) < 0) {
            throw std::runtime_error(std::string("UDP send failed: ") +
                                     std::strerror(errno));
        }
    }

    void read_loop() {
        std::array<std::uint8_t, kMaxDatagram> bytes{};
        while (true) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (stopping) return;
            }
            sockaddr_storage peer{};
            socklen_t peer_length = sizeof(peer);
            const auto received = recvfrom(socket_fd, bytes.data(), bytes.size(), 0,
                                           reinterpret_cast<sockaddr*>(&peer),
                                           &peer_length);
            if (received < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) continue;
                return;
            }
            if (!connected) {
                if (connect(socket_fd, reinterpret_cast<sockaddr*>(&peer), peer_length) < 0) {
                    return;
                }
                connected = true;
            }
            Packet packet{};
            if (!decode(bytes.data(), static_cast<std::size_t>(received), packet)) continue;
            std::lock_guard<std::mutex> lock(mutex);
            if (packet.type == Type::Ack) {
                acknowledged = std::max(acknowledged, packet.acknowledgement);
                congestion_window = std::min<std::size_t>(kReceiveWindow,
                    congestion_window + 1);
                condition.notify_all();
                continue;
            }
            if (packet.sequence < next_receive) {
                transmit(Packet{Type::Ack, 0, next_receive - 1, kReceiveWindow, {}});
                continue;
            }
            if (packet.sequence == next_receive) {
                receive_buffer.insert(receive_buffer.end(), packet.payload.begin(),
                                       packet.payload.end());
                ++next_receive;
                while (out_of_order.count(next_receive) != 0) {
                    const auto& fragment = out_of_order[next_receive];
                    receive_buffer.insert(receive_buffer.end(), fragment.begin(),
                                          fragment.end());
                    const bool end_of_message = fragment.empty();
                    out_of_order.erase(next_receive++);
                    if (end_of_message) {
                        messages.emplace_back(receive_buffer.begin(), receive_buffer.end());
                        receive_buffer.clear();
                        condition.notify_all();
                    }
                }
                // A zero-length fragment marks the end of an application message.
                if (packet.payload.empty()) {
                    messages.emplace_back(receive_buffer.begin(), receive_buffer.end());
                    receive_buffer.clear();
                    condition.notify_all();
                }
            } else if (packet.sequence - next_receive < kReceiveWindow) {
                out_of_order.emplace(packet.sequence, packet.payload);
            }
            transmit(Packet{Type::Ack, 0, next_receive - 1, kReceiveWindow, {}});
        }
    }

    int socket_fd;
    bool connected = false;
    std::thread worker;
    std::mutex mutex;
    std::condition_variable condition;
    bool stopping = false;
    std::uint32_t acknowledged = 0;
    std::uint32_t next_receive = 1;
    std::uint32_t next_send = 1;
    std::size_t congestion_window = 4;
    std::map<std::uint32_t, std::vector<std::uint8_t>> out_of_order;
    std::vector<std::uint8_t> receive_buffer;
    std::deque<std::string> messages;
};

Endpoint::Endpoint(const std::string& peer_host, std::uint16_t peer_port,
                   std::uint16_t local_port)
    : impl_(new Impl(peer_host, peer_port, local_port)) {}

Endpoint::~Endpoint() {
    delete impl_;
}

void Endpoint::send(const std::string& message) {
    std::vector<std::vector<std::uint8_t>> fragments;
    for (std::size_t offset = 0; offset < message.size(); offset += kMaxPayload) {
        const auto count = std::min(kMaxPayload, message.size() - offset);
        fragments.emplace_back(message.begin() + offset, message.begin() + offset + count);
    }
    // Empty messages are represented by the terminator alone.
    fragments.emplace_back();

    std::size_t first = 0;
    std::map<std::uint32_t, std::vector<std::uint8_t>> outstanding;
    while (first < fragments.size()) {
        std::unique_lock<std::mutex> lock(impl_->mutex);
        while (first < fragments.size() &&
               impl_->next_send - impl_->acknowledged <= impl_->congestion_window) {
            const auto sequence = impl_->next_send++;
            const auto payload = fragments[first++];
            outstanding.emplace(sequence, payload);
            lock.unlock();
            impl_->transmit(Packet{Type::Data, sequence, 0, kReceiveWindow, payload});
            lock.lock();
        }
        for (auto it = outstanding.begin(); it != outstanding.end() &&
                                           it->first <= impl_->acknowledged;) {
            it = outstanding.erase(it);
        }
        if (first == fragments.size()) {
            while (impl_->acknowledged + 1 < impl_->next_send) {
                if (impl_->condition.wait_for(lock, kRetransmitTimeout) ==
                    std::cv_status::timeout) {
                    impl_->congestion_window = std::max<std::size_t>(1,
                        impl_->congestion_window / 2);
                    const auto sequence = impl_->acknowledged + 1;
                    const auto packet = outstanding.find(sequence);
                    if (packet == outstanding.end()) {
                        throw std::runtime_error("retransmission state was lost");
                    }
                    lock.unlock();
                    impl_->transmit(Packet{Type::Data, sequence, 0, kReceiveWindow,
                                           packet->second});
                    lock.lock();
                }
                for (auto it = outstanding.begin(); it != outstanding.end() &&
                                                   it->first <= impl_->acknowledged;) {
                    it = outstanding.erase(it);
                }
            }
        } else {
            impl_->condition.wait_for(lock, kRetransmitTimeout);
        }
    }
}

bool Endpoint::receive(std::string& message, int timeout_ms) {
    std::unique_lock<std::mutex> lock(impl_->mutex);
    const auto ready = [&] { return !impl_->messages.empty() || impl_->stopping; };
    if (timeout_ms < 0) {
        impl_->condition.wait(lock, ready);
    } else if (!impl_->condition.wait_for(lock, std::chrono::milliseconds(timeout_ms), ready)) {
        return false;
    }
    if (impl_->messages.empty()) return false;
    message = std::move(impl_->messages.front());
    impl_->messages.pop_front();
    return true;
}

} // namespace rudp
