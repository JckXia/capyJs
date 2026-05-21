#pragma once
#include "i_worker.h"
#include "quickjs.h"
#include "uv.h"
#include <cstddef>
#include <cstdint>
#include <vector>

// IManager<TJob, TResult>
//
// Contract for a worker-aware pool. The manager owns the workers and is the
// only entry point for JS-layer dispatch. It has no uv_* handles of its own —
// libuv lifecycle is entirely the workers' concern.
//
// Responsibilities:
//   - Allocate and hold workers (raw pointers — workers self-delete on stop)
//   - Implement pick_worker() to route jobs (round-robin, least-loaded, etc.)
//   - Track job status for getProgressByWorkerId()
//   - Receive results from workers via on_result() and dispatch JS callbacks
//
// on_result() is called by the worker's static on_result_ready callback on the
// main thread. It is the seam between the native worker world and the JS world.
//
// Workers and managers are typed on the same <TJob, TResult> pair. A manager
// may hold heterogeneous worker implementations (e.g. CPU + GPU) as long as
// they agree on job and result types.

template <typename TJob, typename TResult>
class IManager {
public:
    virtual ~IManager() = default;

    // JS-facing API
    virtual JSValue js_dispatch(JSContext *ctx, JSValue job_val,
                                JSValue callback) = 0;
    virtual JSValue js_get_progress(JSContext *ctx, JSValue job_id_val) = 0;
    virtual JSValue js_close(JSContext *ctx) = 0;

    // Invoked on the main thread by a worker's on_result_ready.
    // Responsible for updating job status and calling the JS callback.
    virtual void on_result(const TResult &r) = 0;

protected:
    // Scheduling policy — implemented by concrete managers.
    virtual IWorker<TJob, TResult> *pick_worker() = 0;

    std::vector<IWorker<TJob, TResult> *> workers_; // raw — workers self-delete
    uint64_t next_job_id_{0};
    size_t   next_worker_idx_{0};
};
