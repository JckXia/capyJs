#pragma once
#include "quickjs.h"
#include <cstddef>
#include <cstdint>

// IWorker<TJob, TResult>
//
// Contract for a self-contained CPU worker. Each concrete implementation owns:
//   - A dedicated OS thread (not from libuv's threadpool)
//   - A uv_async_t doorbell for signalling the main thread
//   - A submission queue (lock-free, blocks when empty)
//   - A result queue (SPSC, main thread drains in on_result_ready)
//
// Threading rules:
//   enqueue()     — main thread only
//   stop()        — main thread only; worker self-deletes asynchronously
//   queue_depth() — main thread only (scheduling hint, not a hard count)
//   on_result_ready() — static, registered with uv_async_init, fires on main thread
//
// The JSValue callback in TJob is opaque to the worker thread. It is carried
// through the queues as plain bytes and only dereferenced on the main thread
// inside on_result_ready via the manager back-pointer.

template <typename TJob, typename TResult>
class IWorker {
public:
    virtual ~IWorker() = default;

    // Submit a job. Returns false if the submission queue is full — caller
    // is responsible for backpressure handling.
    virtual bool enqueue(TJob job, JSValue callback) = 0;

    // Signal the worker to stop, join its thread, drain both queues freeing
    // any pending JS callbacks, then initiate uv_close on the doorbell.
    // The worker self-deletes in the uv_close callback.
    // Caller must not touch this pointer after stop() returns.
    virtual void stop() = 0;

    // Approximate depth of the submission queue. Used by the manager's
    // pick_worker() to make scheduling decisions — not a strict invariant.
    virtual size_t queue_depth() const = 0;

    virtual int worker_id() const = 0;
};
