#include "inference_engine.h"
#include <cstring>
#include <unistd.h>

NativeInferenceManager::NativeInferenceManager(JSContext *js_ctx, uv_loop_t *loop)
    : js_ctx_(js_ctx), loop_(loop) {
    llama_backend_init();
}

bool NativeInferenceManager::add_model(const char *model_class, const char *path,
                                        bool use_mlock) {
    llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0;
    mp.use_mlock    = use_mlock;

    llama_model *model = llama_model_load_from_file(path, mp);
    if (!model) {
        fprintf(stderr, "[engine] failed to load model '%s' from %s\n",
                model_class, path);
        return false;
    }

    models_[model_class] = model;
    fprintf(stderr, "[engine] loaded model '%s'\n", model_class);
    return true;
}

void NativeInferenceManager::spawn_workers(int count, int n_ctx, int n_batch) {
    if (n_batch <= 0) n_batch = n_ctx; // default: match context window

    int total_cores = (int)sysconf(_SC_NPROCESSORS_ONLN) - 1;
    if (total_cores < 1) total_cores = 1;
    int cores_each = total_cores / count;
    if (cores_each < 1) cores_each = 1;

    workers_.reserve(count);
    int core_cursor = 1;
    for (int i = 0; i < count; i++) {
        workers_.push_back(
            new InferenceWorker(i, core_cursor, cores_each, n_ctx, n_batch,
                                this, js_ctx_, loop_));
        core_cursor += cores_each;
    }
}

llama_model *NativeInferenceManager::get_model(const char *model_class) {
    auto it = models_.find(model_class);
    return it != models_.end() ? it->second : nullptr;
}

IWorker<InfJob, InfResult> *NativeInferenceManager::pick_worker() {
    return workers_[next_worker_idx_++ % workers_.size()];
}

JSValue NativeInferenceManager::js_dispatch(JSContext *ctx, JSValue packet_val,
                                              JSValue callback) {
    if (!JS_IsFunction(ctx, callback))
        return JS_ThrowTypeError(ctx, "callback must be a function");
    if (workers_.empty())
        return JS_ThrowInternalError(ctx, "no workers spawned");

    JSValue cls_val    = JS_GetPropertyStr(ctx, packet_val, "model_class");
    JSValue sid_val    = JS_GetPropertyStr(ctx, packet_val, "session_id");
    JSValue cid_val    = JS_GetPropertyStr(ctx, packet_val, "client_id");
    JSValue prompt_val = JS_GetPropertyStr(ctx, packet_val, "prompt");

    const char *cls    = JS_ToCString(ctx, cls_val);
    const char *sid    = JS_ToCString(ctx, sid_val);
    const char *cid    = JS_ToCString(ctx, cid_val);
    const char *prompt = JS_ToCString(ctx, prompt_val);

    JSValue err = JS_UNDEFINED;
    if (!cls || !get_model(cls)) {
        err = JS_ThrowRangeError(ctx, "unknown model_class");
        goto cleanup;
    }

    {
        uint64_t job_id = next_job_id_++;
        InfJob   job{};
        job.job_id = job_id;
        snprintf(job.model_class, sizeof(job.model_class), "%s", cls);
        snprintf(job.session_id,  sizeof(job.session_id),  "%s", sid ? sid : "");
        snprintf(job.client_id,   sizeof(job.client_id),   "%s", cid ? cid : "");
        snprintf(job.prompt,      sizeof(job.prompt),      "%s", prompt ? prompt : "");

        IWorker<InfJob, InfResult> *worker = pick_worker();
        if (!worker->enqueue(job, JS_DupValue(ctx, callback))) {
            err = JS_ThrowInternalError(ctx, "worker %d queue full", worker->worker_id());
            goto cleanup;
        }

        job_status_[job_id] = JobStatus::PENDING;
        err = JS_NewInt64(ctx, (int64_t)job_id);
    }

cleanup:
    JS_FreeCString(ctx, cls);
    JS_FreeCString(ctx, sid);
    JS_FreeCString(ctx, cid);
    JS_FreeCString(ctx, prompt);
    JS_FreeValue(ctx, cls_val);
    JS_FreeValue(ctx, sid_val);
    JS_FreeValue(ctx, cid_val);
    JS_FreeValue(ctx, prompt_val);
    return err;
}

JSValue NativeInferenceManager::js_get_progress(JSContext *ctx, JSValue job_id_val) {
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

JSValue NativeInferenceManager::js_close(JSContext *ctx) {
    for (auto *w : workers_) w->stop();
    workers_.clear();

    for (auto &[cls, model] : models_) llama_model_free(model);
    models_.clear();

    llama_backend_free();
    return ctx ? JS_UNDEFINED : JS_NULL;
}

JSValue NativeInferenceManager::js_list_workers(JSContext *ctx) {
    JSValue arr = JS_NewArray(ctx);
    for (int i = 0; i < (int)workers_.size(); i++) {
        auto *w = static_cast<InferenceWorker *>(workers_[i]);
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "worker_id",
                          JS_NewInt32(ctx, w->worker_id()));
        JS_SetPropertyStr(ctx, obj, "core_start",
                          JS_NewInt32(ctx, w->core_start()));
        JS_SetPropertyStr(ctx, obj, "core_count",
                          JS_NewInt32(ctx, w->core_count()));
        JS_SetPropertyStr(ctx, obj, "queued_work",
                          JS_NewInt32(ctx, (int)w->queue_depth()));
        JS_SetPropertyUint32(ctx, arr, i, obj);
    }
    return arr;
}

void NativeInferenceManager::on_result(const InfResult &r) {
    job_status_[r.job_id] = r.error ? JobStatus::ERROR : JobStatus::DONE;

    JSValue cb = r.callback;

    if (r.error) {
        JSValue err     = JS_NewString(js_ctx_, r.token);
        JSValue args[2] = {err, JS_NULL};
        JSValue ret     = JS_Call(js_ctx_, cb, JS_UNDEFINED, 2, args);
        JS_FreeValue(js_ctx_, ret);
        JS_FreeValue(js_ctx_, err);
    } else {
        JSValue tok     = JS_NewString(js_ctx_, r.token);
        JSValue args[2] = {JS_NULL, tok};
        JSValue ret     = JS_Call(js_ctx_, cb, JS_UNDEFINED, 2, args);
        JS_FreeValue(js_ctx_, ret);
        JS_FreeValue(js_ctx_, tok);
    }

    if (r.is_done)
        JS_FreeValue(js_ctx_, cb);
}
