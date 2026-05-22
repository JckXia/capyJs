// io_throughput/node_server.js
// Plain HTTP throughput — no compute, no workers.
// Baseline: how fast can the server accept and respond to requests.

const http = require("http");

http.createServer((req, res) => {
    res.writeHead(200, { "Content-Type": "text/plain" });
    res.end("Hello");
}).listen(3000, () => {
    console.log("node io_throughput on :3000");
});
