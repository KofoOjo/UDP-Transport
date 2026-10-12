# Reliable UDP Transport

**Full-stack C++17/Node.js networking project implementing reliable, ordered message delivery on top of raw UDP.**

This project demonstrates end-to-end systems engineering by adding
reliability, sequencing, flow control, congestion control, integrity checks,
and message framing to connectionless UDP, then exposing the transport through
a browser-based WebSocket application.

## Resume highlights

- Designed a reliable transport protocol with 32-bit sequence numbers,
  cumulative acknowledgements, bounded out-of-order buffering, and in-order
  message reassembly.
- Implemented retransmission with a fixed 120 ms timeout and congestion control
  using additive increase and multiplicative decrease.
- Added MTU-aware fragmentation and explicit message-boundary framing so
  application messages can span multiple datagrams.
- Defined and implemented a compact 22-byte binary wire header containing
  protocol metadata, receive-window advertisement, payload length, and an
  FNV-1a integrity checksum.
- Built a command-line client/server demo and a small reusable endpoint library
  with CMake and compiler warnings enabled.
- Integrated the C++ transport with an Express/WebSocket service and a
  responsive browser client, creating a complete browser-to-UDP data path.

## Technical focus

`C++17` · `UDP sockets` · `network protocols` · `binary serialization` ·
`retransmission` · `congestion control` · `flow control` · `CMake` ·
`Linux/macOS networking APIs` · `Node.js` · `Express` · `WebSocket` · `HTML/CSS/JavaScript`

## Build

Requirements: CMake 3.16 or newer, a C++17-compatible compiler, and Node.js
18 or newer for the web demo. On macOS with Homebrew, install Node.js with:

```sh
brew install node
```

```sh
cmake -S . -B build
cmake --build build
```

## Run the demo

Start the server in one terminal:

```sh
./build/reliable_udp_demo server 9000
```

Connect with the client from another terminal:

```sh
./build/reliable_udp_demo client 127.0.0.1 9000
```

Type lines into the client. The transport fragments, transmits, acknowledges,
reassembles, and delivers each message reliably; the server responds with an
acknowledgement.

## Full-stack web demo

Run these commands from the repository root (`UDP Transport`), not from the
`build` directory:

Install the web dependencies:

```sh
npm install
```

Start the UDP server in one terminal:

```sh
./build/reliable_udp_demo server 9000
```

Start the web application in another:

```sh
npm start
```

Open <http://localhost:3000>. The browser sends JSON messages over WebSocket
to the Node.js server, which starts a C++ `udp_bridge` process for the session.
The bridge uses the reusable `reliable_udp` library to deliver each message
over UDP and return the acknowledgement to the browser.

The web app can target a different UDP server with environment variables:

```sh
UDP_HOST=127.0.0.1 UDP_PORT=9000 npm start
```

## Protocol design

Each datagram contains a 22-byte header with the magic value, protocol
version, packet type, sequence number, cumulative acknowledgement, advertised
receive window, payload length, and checksum. Data packets use one sequence
number per fragment. An empty data packet terminates each application message,
allowing the receiver to preserve message boundaries without exposing
transport framing details to callers.

## Scope and future improvements

This is an educational transport implementation focused on clear protocol
mechanics rather than production deployment. Potential next steps include:

- adaptive RTT measurement and retransmission timeouts;
- authenticated encryption and peer authentication;
- support for multiple concurrent peers;
- automated loss-injection tests and throughput/latency benchmarks.
