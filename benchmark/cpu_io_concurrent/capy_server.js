// cpu_io_concurrent/capy_server.js
//
// THE KEY BENCHMARK: measure HTTP latency while 5 CPU workers are fully saturated.
//
// Workers run on dedicated OS threads created with uv_thread_create — completely
// separate from libuv's internal threadpool. No matter how hard the workers run,
// the I/O event loop never contends with them for a threadpool slot.
//
// Run alongside node_server.js (different port) and hit both with wrk while
// workers are saturated. Compare P99 latency under load.

const WORKERS = 5;
const PRIME_LIMIT = 100000;

const mgr = new JSWorkerManager({
    worker_size: WORKERS,
    worker_js: "./benchmark/cpu_io_concurrent/capy_worker.js",
});

// Keep all workers continuously saturated with CPU work.
function saturate() {
    mgr.dispatch(JSON.stringify({ limit: PRIME_LIMIT }), (err, result) => {
        saturate(); // re-queue immediately on completion — keeps workers hot
    });
}
for (let i = 0; i < WORKERS; i++) saturate();

const server = new Server();

// HTTP endpoint — this should stay low-latency even with workers pegged.
server.get("/ping", (req, res) => {
    res.setHeader("Content-Type", "text/plain").end("pong");
});

// Also expose a dispatch endpoint to benchmark job round-trip latency.
server.get("/job", (req, res) => {
    mgr.dispatch(JSON.stringify({ limit: PRIME_LIMIT }), (err, result) => {
        res.setHeader("Content-Type", "application/json").end(result || err);
    });
});

server.listen(3001, () => {
    console.log(`capy cpu_io_concurrent on :3001 (${WORKERS} workers saturated)`);
});
