// node_fib_worker.js — worker_threads entry point.
// V8 will JIT-optimize fib() after warmup — this is Node at its best.

const { parentPort } = require("worker_threads");

function fib(n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

parentPort.on("message", ({ jobId, n }) => {
    const result = fib(n);
    parentPort.postMessage({ jobId, result });
});
