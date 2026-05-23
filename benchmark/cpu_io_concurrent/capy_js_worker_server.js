// capy_js_worker_server.js — ACT 1
//
// JSWorkerManager backed by QuickJS interpreters (no JIT).
// Expected to lose to Node worker_threads by ~10x on /job.
// This is intentional — QuickJS has no JIT, this is the honest cost.
// Compare against capy_native_worker_server.js to see the real story.

const WORKERS = 5;

const mgr = new JSWorkerManager({
    worker_size: WORKERS,
    worker_js: './benchmark/cpu_io_concurrent/capy_fib_worker.js',
});

const server = new Server();

server.get("/job", (req, res) => {
    mgr.dispatch(JSON.stringify({ n: 35 }), (err, result) => {
        res.setHeader("Content-Type", "application/json")
           .end(result || JSON.stringify({ error: err }));
    });
});

server.get("/ping", (req, res) => {
    res.setHeader("Content-Type", "text/plain").end("pong");
});

server.listen(3001, () => {
    console.log(`capy JSWorkerManager on :3001 (${WORKERS} QuickJS workers)`);
});
