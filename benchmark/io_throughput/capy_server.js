// io_throughput/capy_server.js
// Plain HTTP throughput — no compute, no workers.
// Baseline: how fast can the server accept and respond to requests.

const server = new Server();

server.get("/", (req, res) => {
    res.setHeader("Content-Type", "text/plain").end("Hello");
});

server.listen(3001, () => {
    console.log("capy io_throughput on :3001");
});
