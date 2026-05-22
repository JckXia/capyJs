// Fibonacci — CPU-bound offloading example
//
// FibonacciManager spawns N dedicated OS threads, each owning its own
// uv_async_t doorbell. Jobs are dispatched from the main thread and run
// entirely off the event loop. Results are signalled back via the doorbell
// and delivered to the JS callback on the main thread — no libuv threadpool
// involvement, no contention with I/O.
//
// The manager's lifecycle is independent of any individual job. Workers
// determine when they are done and signal back; the manager never polls.
// This same pattern slots directly into streaming workloads (e.g. LLM
// inference) where a worker signals the main thread multiple times per job.

const mgr = new FibonacciManager({ worker_size: 3 });

// dispatch() is non-blocking. It enqueues the job onto a worker's lock-free
// submission queue and returns a jobId immediately. The callback fires on the
// main thread once the worker signals its doorbell.
const j0 = mgr.dispatch({ value: 10 }, (err, result) => {
    if (err) return console.log("j0 error:", err);
    console.log("fib(10) =", result);
    console.log("j0 progress:", mgr.getProgressByWorkerId(j0)); // "done"
});

const j1 = mgr.dispatch({ value: 20 }, (err, result) => {
    if (err) return console.log("j1 error:", err);
    console.log("fib(20) =", result);
});

const j2 = mgr.dispatch({ value: 35 }, (err, result) => {
    if (err) return console.log("j2 error:", err);
    console.log("fib(35) =", result);

    // Closing from inside a callback is valid. stop() joins each worker thread
    // synchronously; workers drain their own queues and self-destruct via
    // uv_close. The manager itself has no libuv handles — it simply exits.
    mgr.close();
});

// Progress is tracked by the manager from dispatch until the callback fires.
console.log("j0 progress after dispatch:", mgr.getProgressByWorkerId(j0)); // "pending"
