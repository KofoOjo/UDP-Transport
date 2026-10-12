const status = document.querySelector("#status");
const form = document.querySelector("#message-form");
const input = document.querySelector("#message");
const events = document.querySelector("#events");
const socket = new WebSocket(`ws://${window.location.host}`);

function addEvent(label, message, error = false) {
  const item = document.createElement("div");
  item.className = `event${error ? " error" : ""}`;
  item.textContent = `${label}: ${message}`;
  events.prepend(item);
}

socket.addEventListener("open", () => {
  status.textContent = "Connected";
  status.classList.add("connected");
});

socket.addEventListener("close", () => {
  status.textContent = "Disconnected";
  status.classList.remove("connected");
});

socket.addEventListener("message", (event) => {
  let response;
  try {
    response = JSON.parse(event.data);
  } catch {
    addEvent("Error", "Invalid response from web server", true);
    return;
  }
  if (response.ok) addEvent("Server acknowledgement", response.message);
  else addEvent("Error", response.error, true);
});

form.addEventListener("submit", (event) => {
  event.preventDefault();
  if (socket.readyState !== WebSocket.OPEN || !input.value.trim()) return;
  addEvent("Sent", input.value);
  socket.send(JSON.stringify({ message: input.value }));
  input.value = "";
  input.focus();
});
