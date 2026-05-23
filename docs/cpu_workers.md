# CPU Workers

Capy offloads CPU-intensive work to dedicated OS threads with a `uv_async_t` doorbell
as the only path back to the main event loop. The main thread never blocks waiting
on compute results.

## Architecture

```
main thread (libuv event loop)
    |
    |-- dispatch(job) ──> WorkQueue (lock-free SPSC + semaphore)
    |                          |
    |                    worker thread (dedicated OS thread)
    |                          |
    |                    result_queue (lock-free SPSC)
    |                          |
    |<── uv_async_send() ──────┘
    |
    on_result_ready() → JS callback
```

Each worker owns:
- A dedicated OS thread (`uv_thread_create`, not libuv's threadpool)
- A submission queue (`WorkQueue`) — blocks on semaphore when empty, zero busy-polling
- A result queue (`SPSCQueue`) — lock-free single-producer single-consumer
- A `uv_async_t` doorbell — signals the main thread when results are ready

Workers are completely invisible to libuv's internal threadpool. `fs`, DNS, and other
libuv I/O operations are unaffected regardless of how many workers are running or how
saturated they are.

## Native Workers (FibonacciManager)

The reference implementation. Workers run native C++ compute.

```js
const mgr = new FibonacciManager({ worker_size: 4 });

const jobId = mgr.dispatch({ value: 35 }, (err, result) => {
    if (err) return console.log("error:", err);
    console.log("fib(35) =", result);
    mgr.close();
});

console.log("status:", mgr.getProgressByWorkerId(jobId)); // "pending"
```

**Constructor options:**
- `worker_size` — number of worker threads

**Methods:**
- `mgr.dispatch({ value: N }, callback)` — dispatches a job, returns a jobId
- `mgr.getProgressByWorkerId(jobId)` — returns `"pending"`, `"done"`, or `"error"`
- `mgr.close()` — stops all workers and frees resources. Safe to call from inside a callback.

## JS Workers (JSWorkerManager)

Each worker spins up an isolated QuickJS runtime (no event loop). The worker script
must define a global `process(inputJson)` function that returns a JSON string.
JSON is the explicit serialization boundary between runtimes.

```js
const mgr = new JSWorkerManager({
    worker_size: 4,
    worker_js: './worker.js',
});

mgr.dispatch(JSON.stringify({ limit: 10000 }), (err, result) => {
    if (!err) console.log(JSON.parse(result));
    mgr.close();
});
```

**worker.js:**
```js
function process(inputJson) {
    const { limit } = JSON.parse(inputJson);
    // ... CPU work ...
    return JSON.stringify({ result });
}
```

**Constructor options:**
- `worker_size` — number of worker threads
- `worker_js` — path to the worker script

**Methods:**
- `mgr.dispatch(inputJson, callback)` — dispatches a job, returns a jobId
- `mgr.getProgress(jobId)` — returns `"pending"`, `"done"`, or `"error"`
- `mgr.close()` — stops all workers and frees resources

## Choosing between native and JS workers

| | Native | JS |
|---|---|---|
| Performance | C++ speed | QuickJS interpreter (~10x slower than V8) |
| Use case | Production compute | Prototyping, sandboxed user code |
| Worker code | C++ implementing IWorker | JavaScript process() function |
| Cross-boundary serialization | Typed structs | JSON strings |

For production CPU-bound workloads, native workers are the intended path.
JS workers exist to validate the interface and for cases where isolation matters
more than performance.

## Lifecycle and safety

Workers are dispatched from the main thread only. The `JSValue` callback is carried
through queues as opaque bytes and only dereferenced on the main thread inside
`on_result_ready`. Worker threads never touch the JS heap.

`mgr.close()` joins all worker threads, drains pending queues, and frees JS callbacks
before returning. It is safe to call from inside a dispatch callback.
