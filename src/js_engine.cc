#include "js_engine.h"

NativeJSWorkerManager::NativeJSWorkerManager(JSContext *js_ctx,
                                              uv_loop_t *loop,
                                              int worker_size,
                                              const std::string &worker_js_path)
    : js_ctx_(js_ctx), loop_(loop), worker_js_path_(worker_js_path) {
    workers_.reserve(worker_size);
    for (int i = 0; i < worker_size; i++)
        workers_.push_back(
            new JSWorker(i, this, js_ctx_, loop_, worker_js_path_));
}

IWorker<JSJob, JSResult> *NativeJSWorkerManager::pick_worker() {
    return workers_[next_worker_idx_++ % workers_.size()];
}

JSValue NativeJSWorkerManager::js_dispatch(JSContext *ctx,
                                            JSValue input_val,
                                            JSValue callback) {
    if (!JS_IsFunction(ctx, callback))
        return JS_ThrowTypeError(ctx, "callback must be a function");
    if (workers_.empty())
        return JS_ThrowInternalError(ctx, "no workers");

    const char *input_str = JS_ToCString(ctx, input_val);
    if (!input_str)
        return JS_ThrowTypeError(ctx, "input must be a string (JSON)");

    uint64_t    job_id = next_job_id_++;
    JSJob       job{job_id, std::string(input_str)};
    JS_FreeCString(ctx, input_str);

    IWorker<JSJob, JSResult> *worker = pick_worker();
    if (!worker->enqueue(job, JS_DupValue(ctx, callback)))
        return JS_ThrowInternalError(ctx, "worker %d queue full",
                                     worker->worker_id());

    job_status_[job_id] = JobStatus::PENDING;
    return JS_NewInt64(ctx, (int64_t)job_id);
}

JSValue NativeJSWorkerManager::js_get_progress(JSContext *ctx,
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

JSValue NativeJSWorkerManager::js_close(JSContext *ctx) {
    for (auto *w : workers_)
        w->stop();
    workers_.clear();
    return ctx ? JS_UNDEFINED : JS_NULL;
}

void NativeJSWorkerManager::on_result(const JSResult &r) {
    job_status_[r.job_id] = r.error ? JobStatus::ERROR : JobStatus::DONE;

    JSValue cb = r.callback;

    if (r.error) {
        JSValue err     = JS_NewString(js_ctx_, r.error_msg.c_str());
        JSValue args[2] = {err, JS_NULL};
        JSValue ret     = JS_Call(js_ctx_, cb, JS_UNDEFINED, 2, args);
        JS_FreeValue(js_ctx_, ret);
        JS_FreeValue(js_ctx_, err);
    } else {
        JSValue out     = JS_NewString(js_ctx_, r.output_json.c_str());
        JSValue args[2] = {JS_NULL, out};
        JSValue ret     = JS_Call(js_ctx_, cb, JS_UNDEFINED, 2, args);
        JS_FreeValue(js_ctx_, ret);
        JS_FreeValue(js_ctx_, out);
    }

    JS_FreeValue(js_ctx_, cb);
}
