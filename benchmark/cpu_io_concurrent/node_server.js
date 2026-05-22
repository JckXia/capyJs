// cpu_io_concurrent/node_server.js
//
// THE KEY BENCHMARK: measure HTTP latency while 5 worker_threads are fully saturated.
//
// Node worker_threads are real OS threads — same as Capy's model. However,
// libuv's internal threadpool (fs, dns, crypto) shares CPU resources with
// everything else on the machine. worker_threads themselves don't steal from
// the pool, but they do contend for CPU cores with the event loop thread.
//
// Run alongside capy_server.js and compare P99 latency under identical load.

const http = require("http");
const { Worker } = require("worker_threads");
const path = require("path");

const WORKERS = 5;
const PRIME_LIMIT = 100000;

// Simple round-robin pool over worker_threads.
const workers = [];
const pending = new Map();
let nextJobId = 0;
let nextWorker = 0;

for (let i = 0; i < WORKERS; i++) {
    const w = new Worker(path.join(__dirname, "node_worker.js"));
    w.on("message", ({ jobId, primes, limit }) => {
        const resolve = pending.get(jobId);
        if (resolve) { pending.delete(jobId); resolve({ primes, limit }); }
    });
    workers.push(w);
}

function dispatch(limit) {
    return new Promise((resolve) => {
        const jobId = nextJobId++;
        pending.set(jobId, resolve);
        workers[nextWorker++ % WORKERS].postMessage({ jobId, limit });
    });
}

// Keep all workers continuously saturated.
function saturate() {
    dispatch(PRIME_LIMIT).then(saturate);
}
for (let i = 0; i < WORKERS; i++) saturate();

http.createServer(async (req, res) => {
    if (req.url === "/ping") {
        res.writeHead(200, { "Content-Type": "text/plain" });
        res.end("pong");
        return;
    }
    if (req.url === "/job") {
        const result = await dispatch(PRIME_LIMIT);
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify(result));
        return;
    }
    res.writeHead(404);
    res.end();
}).listen(3000, () => {
    console.log(`node cpu_io_concurrent on :3000 (${WORKERS} workers saturated)`);
});
