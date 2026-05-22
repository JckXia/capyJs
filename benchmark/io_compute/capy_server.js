// io_compute/capy_server.js
// HTTP + JSON parse + serialize per request.
// Represents a realistic API server: deserialize input, do light work, respond.

const server = new Server();

server.get("/compute", (req, res) => {
    const payload = { user: "bench", ts: Date.now(), values: [1, 2, 3, 4, 5] };
    const serialized = JSON.stringify(payload);
    const parsed = JSON.parse(serialized);
    const result = JSON.stringify({
        sum: parsed.values.reduce((a, b) => a + b, 0),
        ts: parsed.ts,
    });

    res.setHeader("Content-Type", "application/json").end(result);
});

server.listen(3001, () => {
    console.log("capy io_compute on :3001");
});
