// node_server.js — ACT 1 comparison
//
// Node worker_threads running recursive fibonacci.
// V8 JIT will warm up and optimize fib() — this is Node at its best.
// Compare /job throughput against capy_js_worker_server.js (Act 1)
// and capy_native_worker_server.js (Act 2).

const http = require("http");
const { Worker } = require("worker_threads");
const path = require("path");

const WORKERS = 5;

const workers = [];
const pending = new Map();
let nextJobId = 0;
let nextWorker = 0;

for (let i = 0; i < WORKERS; i++) {
    const w = new Worker(path.join(__dirname, "node_fib_worker.js"));
    w.on("message", ({ jobId, result }) => {
        const resolve = pending.get(jobId);
        if (resolve) { pending.delete(jobId); resolve(result); }
    });
    workers.push(w);
}

function dispatch(n) {
    return new Promise((resolve) => {
        const jobId = nextJobId++;
        pending.set(jobId, resolve);
        workers[nextWorker++ % WORKERS].postMessage({ jobId, n });
    });
}

http.createServer(async (req, res) => {
    if (req.url === "/job") {
        const result = await dispatch(35);
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ n: 35, result }));
        return;
    }
    if (req.url === "/ping") {
        res.writeHead(200, { "Content-Type": "text/plain" });
        res.end("pong");
        return;
    }
    res.writeHead(404);
    res.end();
}).listen(3000, () => {
    console.log(`node worker_threads on :3000 (${WORKERS} workers, V8 JIT)`);
});
