// js_worker.js — demonstrates JSWorkerManager.
//
// Each worker spins up its own QuickJS runtime (no event loop).
// The worker_js file must define a global process(inputJson) → JSON string.
// Data crosses the runtime boundary as JSON strings — the unavoidable tax
// of keeping two JS engines from sharing a heap.

const mgr = new JSWorkerManager({
    worker_size: 4,
    worker_js: './examples/worker_process.js',
});

let done = 0;
const total = 8;

for (let i = 0; i < total; i++) {
    const limit = 10000 + i * 5000;
    const jobId = mgr.dispatch(JSON.stringify({ limit }), (err, result) => {
        if (err) {
            console.log('error:', err);
        } else {
            const r = JSON.parse(result);
            console.log(`limit=${r.limit} → ${r.primes} primes`);
        }
        if (++done === total) mgr.close();
    });
    console.log(`dispatched job ${jobId} (limit=${limit})`);
}
