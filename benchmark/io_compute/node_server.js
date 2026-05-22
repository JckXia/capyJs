// io_compute/node_server.js
// HTTP + JSON parse + serialize per request.
// Represents a realistic API server: deserialize input, do light work, respond.

const http = require("http");

http.createServer((req, res) => {
    const payload = { user: "bench", ts: Date.now(), values: [1, 2, 3, 4, 5] };
    const serialized = JSON.stringify(payload);
    const parsed = JSON.parse(serialized);
    const result = JSON.stringify({
        sum: parsed.values.reduce((a, b) => a + b, 0),
        ts: parsed.ts,
    });

    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(result);
}).listen(3000, () => {
    console.log("node io_compute on :3000");
});
