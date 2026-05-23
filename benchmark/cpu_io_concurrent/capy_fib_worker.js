// capy_fib_worker.js — runs inside each JSWorker's isolated QuickJS runtime.
// Intentionally no JIT — this is the baseline for Act 1.

function fib(n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

function process(inputJson) {
    const { n } = JSON.parse(inputJson);
    const result = fib(n);
    return JSON.stringify({ n, result });
}
