#include "fib_engine.h"

NativeFibonacciManager::NativeFibonacciManager(JSContext *js_ctx,
                                                uv_loop_t *loop,
                                                int worker_size)
    : js_ctx_(js_ctx), loop_(loop) {
    workers_.reserve(worker_size);
    for (int i = 0; i < worker_size; i++)
        workers_.push_back(
            new FibonacciWorker(i, this, js_ctx_, loop_));
}

IWorker<FibJob, FibResult> *NativeFibonacciManager::pick_worker() {
    return workers_[next_worker_idx_++ % workers_.size()];
}

JSValue NativeFibonacciManager::js_dispatch(JSContext *ctx,
                                              JSValue value_val,
                                              JSValue callback) {
    if (!JS_IsFunction(ctx, callback))
        return JS_ThrowTypeError(ctx, "callback must be a function");
    if (workers_.empty())
        return JS_ThrowInternalError(ctx, "no workers");

    uint64_t value = 0;
    JS_ToIndex(ctx, &value, value_val);

    uint64_t job_id = next_job_id_++;
    FibJob   job{job_id, value};

    IWorker<FibJob, FibResult> *worker = pick_worker();
    if (!worker->enqueue(job, JS_DupValue(ctx, callback)))
        return JS_ThrowInternalError(ctx, "worker %d queue full",
                                     worker->worker_id());

    job_status_[job_id] = JobStatus::PENDING;
    return JS_NewInt64(ctx, (int64_t)job_id);
}

JSValue NativeFibonacciManager::js_get_progress(JSContext *ctx,
                                                  JSValue job_id_val) {
    uint64_t job_id = 0;
    JS_ToIndex(ctx, &job_id, job_id_val);

    auto it = job_status_.find(job_id);
    if (it == job_status_.end()) return JS_NewString(ctx, "unknown");

    switch (it->second) {
    case JobStatus::PENDING: return JS_NewString(ctx, "pending");
    case JobStatus::DONE:    return JS_NewString(ctx, "done");
    case JobStatus::ERROR:   return JS_NewString(ctx, "error");
    }
    return JS_NewString(ctx, "unknown");
}

JSValue NativeFibonacciManager::js_close(JSContext *ctx) {
    for (auto *w : workers_)
        w->stop();
    workers_.clear();
    return ctx ? JS_UNDEFINED : JS_NULL; // ctx is null when called from GC finalizer
}

void NativeFibonacciManager::on_result(const FibResult &r) {
    job_status_[r.job_id] = r.error ? JobStatus::ERROR : JobStatus::DONE;

    JSValue cb = r.callback;

    if (r.error) {
        JSValue err     = JS_NewString(js_ctx_, "fibonacci error");
        JSValue args[2] = {err, JS_NULL};
        JSValue ret     = JS_Call(js_ctx_, cb, JS_UNDEFINED, 2, args);
        JS_FreeValue(js_ctx_, ret);
        JS_FreeValue(js_ctx_, err);
    } else {
        JSValue val     = JS_NewFloat64(js_ctx_, (double)r.result);
        JSValue args[2] = {JS_NULL, val};
        JSValue ret     = JS_Call(js_ctx_, cb, JS_UNDEFINED, 2, args);
        JS_FreeValue(js_ctx_, ret);
        JS_FreeValue(js_ctx_, val);
    }

    JS_FreeValue(js_ctx_, cb);
}
