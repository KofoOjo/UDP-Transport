# Reliable UDP transport

This is a small C++17 teaching implementation of reliable delivery over raw
UDP. It provides:

- 32-bit sequence numbers and cumulative ACKs
- in-order reassembly with a bounded out-of-order receive window
- retransmission after a 120 ms timeout
- a congestion window with additive growth and multiplicative decrease
- application messages split into MTU-sized fragments
- an FNV-1a checksum to discard corrupted datagrams

The transport intentionally omits encryption, peer authentication, NAT
traversal, and production-grade RTT estimation. UDP loss can be simulated with
`tc`, `dummynet`, or a proxy to observe retransmission and congestion behavior.

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Run the demo

In one terminal:

```sh
./build/reliable_udp_demo server 9000
```

In another:

```sh
./build/reliable_udp_demo client 127.0.0.1 9000
```

Type lines into the client. Each line is delivered reliably and the server
returns an acknowledgement message.

## Wire format

Every datagram has a 22-byte header: magic, version, packet type, sequence
number, cumulative acknowledgement, advertised receive window, payload
length, and checksum. Data packets use one sequence number per fragment.
An empty data packet terminates an application message, allowing the receiver
to distinguish message boundaries without adding an application framing API.

## Limitations and future work

This project intentionally favors clear transport concepts over production
features:

- The retransmission timeout is fixed at 120 ms rather than using adaptive RTT
  estimation. Real TCP and QUIC measure round-trip time continuously and
  adjust their timeout accordingly.
- Packets are not encrypted and peers are not authenticated. A production
  transport would need authenticated encryption and a key-exchange strategy.
- The demo supports one connected peer at a time rather than managing many
  concurrent peers under load.
- No benchmark suite is included yet. A useful next step would be to measure
  throughput and latency under induced packet loss, producing concrete results
  such as recovery time at a given loss percentage.
