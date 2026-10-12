const express = require("express");
const http = require("http");
const path = require("path");
const { spawn } = require("child_process");
const { WebSocketServer } = require("ws");
const readline = require("readline");

const HTTP_PORT = Number(process.env.HTTP_PORT || 3000);
const UDP_HOST = process.env.UDP_HOST || "127.0.0.1";
const UDP_PORT = Number(process.env.UDP_PORT || 9000);
const BRIDGE_PATH = process.env.BRIDGE_PATH || path.join(__dirname, "build", "udp_bridge");

const app = express();
app.use(express.static(path.join(__dirname, "public")));
const server = http.createServer(app);
const webSockets = new WebSocketServer({ server });

webSockets.on("connection", (socket) => {
  const bridge = spawn(BRIDGE_PATH, [UDP_HOST, String(UDP_PORT)]);
  const output = readline.createInterface({ input: bridge.stdout });

  const send = (payload) => {
    if (socket.readyState === socket.OPEN) socket.send(JSON.stringify(payload));
  };

  output.on("line", (line) => {
    try {
      send(JSON.parse(line));
    } catch {
      send({ ok: false, error: "Invalid response from UDP bridge" });
    }
  });

  bridge.stderr.on("data", (chunk) => {
    console.error(`[udp_bridge] ${chunk.toString().trim()}`);
  });

  bridge.on("error", (error) => {
    send({ ok: false, error: `Unable to start UDP bridge: ${error.message}` });
  });

  bridge.on("exit", (code) => {
    if (code !== 0) send({ ok: false, error: "UDP bridge stopped unexpectedly" });
  });

  socket.on("message", (data) => {
    let request;
    try {
      request = JSON.parse(data.toString());
    } catch {
      send({ ok: false, error: "Messages must be valid JSON" });
      return;
    }
    if (typeof request.message !== "string" || request.message.length > 65536) {
      send({ ok: false, error: "message must be a string of at most 65536 characters" });
      return;
    }
    bridge.stdin.write(`${request.message.replace(/\r?\n/g, " ")}\n`);
  });

  socket.on("close", () => {
    output.close();
    bridge.kill();
  });
});

server.listen(HTTP_PORT, () => {
  console.log(`Web app: http://localhost:${HTTP_PORT}`);
  console.log(`UDP target: ${UDP_HOST}:${UDP_PORT}`);
});
