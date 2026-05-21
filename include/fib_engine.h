#pragma once
#include "fib_worker.h"
#include "quickjs.h"
#include "uv.h"
#include <cstdint>
#include <map>
#include <vector>

enum class JobStatus { PENDING, DONE, ERROR };

class NativeFibonacciManager {
public:
    NativeFibonacciManager(JSContext *js_ctx, uv_loop_t *loop, int worker_size);

    NativeFibonacciManager(const NativeFibonacciManager &) = delete;
    NativeFibonacciManager &operator=(const NativeFibonacciManager &) = delete;

    // JS-facing methods (main thread only)
    JSValue js_dispatch(JSContext *ctx, JSValue value_val, JSValue callback);
    JSValue js_get_progress(JSContext *ctx, JSValue job_id_val);
    JSValue js_close(JSContext *ctx);

    // Called by FibonacciWorker::on_result_ready on the main thread.
    void on_result(const FibResult &r);

private:
    FibonacciWorker *pick_worker();

    std::vector<FibonacciWorker *> workers_; // raw — workers self-delete on stop()
    std::map<uint64_t, JobStatus>  job_status_;

    JSContext *js_ctx_;
    uv_loop_t *loop_;
    uint64_t   next_job_id_{0};
    size_t     next_worker_idx_{0};
};
