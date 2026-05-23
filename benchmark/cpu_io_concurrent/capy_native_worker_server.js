// capy_native_worker_server.js — ACT 2
//
// Same /job endpoint, now backed by NativeFibonacciManager.
// Workers run native C++ fibonacci — no interpreter, no JIT warmup.
// Compare /job throughput against capy_js_worker_server.js and node_server.js.

const WORKERS = 5;

const mgr = new FibonacciManager({ worker_size: WORKERS });

const server = new Server();

server.get("/job", (req, res) => {
    mgr.dispatch({ value: 35 }, (err, result) => {
        res.setHeader("Content-Type", "application/json")
           .end(JSON.stringify({ n: 35, result }));
    });
});

server.get("/ping", (req, res) => {
    res.setHeader("Content-Type", "text/plain").end("pong");
});

server.listen(3001, () => {
    console.log(`capy NativeFibonacciManager on :3001 (${WORKERS} native workers)`);
});
