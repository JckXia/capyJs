#include "inference_engine.h"
#include "llama.h"
#include <cstring>
#include <iostream>
#include <unistd.h>

InferenceEngine::InferenceEngine(JSContext *js_ctx, uv_loop_t *loop)
    : js_ctx_(js_ctx) {
    llama_backend_init();
    uv_async_init(loop, &async_, on_token_ready);
    uv_unref((uv_handle_t *)&async_); // idle — don't block loop exit
    async_.data = this;
}

void InferenceEngine::shutdown() {
    for (auto &w : workers_) w->stop();
    for (auto &[cls, model] : models_) llama_model_free(model);
    models_.clear();
    llama_backend_free();
}

bool InferenceEngine::add_model(const char *model_class, const char *path) {
    llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0;

    llama_model *model = llama_model_load_from_file(path, mp);
    if (!model) {
        fprintf(stderr, "[engine] failed to load model '%s' from %s\n", model_class, path);
        return false;
    }

    models_[model_class] = model;
    fprintf(stderr, "[engine] loaded model '%s'\n", model_class);
    return true;
}

void InferenceEngine::spawn_workers(int count) {
    int total_cores  = (int)sysconf(_SC_NPROCESSORS_ONLN) - 1; // exclude core 0
    if (total_cores < 1) total_cores = 1;
    int cores_each   = total_cores / count;
    if (cores_each < 1) cores_each = 1;

    int core_cursor = 1; // start at core 1
    for (int i = 0; i < count; i++) {
        workers_.push_back(
            std::make_unique<InferenceWorker>(i, core_cursor, cores_each, this));
        workers_.back()->start();
        core_cursor += cores_each;
    }
}

llama_model *InferenceEngine::get_model(const char *model_class) {
    auto it = models_.find(model_class);
    return it != models_.end() ? it->second : nullptr;
}

JSValue InferenceEngine::js_list_workers(JSContext *ctx) {
    JSValue arr = JS_NewArray(ctx);
    for (int i = 0; i < (int)workers_.size(); i++) {
        WorkerStats s = workers_[i]->stats();
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "worker_id",   JS_NewInt32(ctx, s.worker_id));
        JS_SetPropertyStr(ctx, obj, "core_start",  JS_NewInt32(ctx, s.core_start));
        JS_SetPropertyStr(ctx, obj, "core_count",  JS_NewInt32(ctx, s.core_count));
        JS_SetPropertyStr(ctx, obj, "queued_work", JS_NewInt32(ctx, s.queued_depth));
        JS_SetPropertyUint32(ctx, arr, i, obj);
    }
    return arr;
}

JSValue InferenceEngine::js_do_inference(JSContext *ctx, JSValue model_class_val,
                                         JSValue packet, JSValue callback) {
    if (workers_.empty())
        return JS_ThrowInternalError(ctx, "no workers spawned");

    if (!JS_IsFunction(ctx, callback))
        return JS_ThrowTypeError(ctx, "callback must be a function");

    const char *model_class = JS_ToCString(ctx, model_class_val);
    if (!model_class)
        return JS_ThrowTypeError(ctx, "model_class must be a string");

    if (!get_model(model_class)) {
        JS_FreeCString(ctx, model_class);
        return JS_ThrowRangeError(ctx, "unknown model_class");
    }

    JSValue sid_val    = JS_GetPropertyStr(ctx, packet, "session_id");
    JSValue prompt_val = JS_GetPropertyStr(ctx, packet, "prompt");

    const char *sid    = JS_ToCString(ctx, sid_val);
    const char *prompt = JS_ToCString(ctx, prompt_val);

    JobItem job{};
    job.job_id = next_job_id_++;
    snprintf(job.model_class, sizeof(job.model_class), "%s", model_class);
    snprintf(job.session_id,  sizeof(job.session_id),  "%s", sid    ? sid    : "default");
    snprintf(job.prompt,      sizeof(job.prompt),      "%s", prompt ? prompt : "");

    JS_FreeCString(ctx, model_class);
    JS_FreeCString(ctx, sid);
    JS_FreeCString(ctx, prompt);
    JS_FreeValue(ctx, sid_val);
    JS_FreeValue(ctx, prompt_val);

    // Round-robin across workers
    int worker_id = next_worker_rr_;
    next_worker_rr_ = (next_worker_rr_ + 1) % (int)workers_.size();

    bool was_idle = pending_callbacks_.empty();
    pending_callbacks_[job.job_id] = JS_DupValue(ctx, callback);

    if (!workers_[worker_id]->enqueue_job(job)) {
        JS_FreeValue(ctx, pending_callbacks_[job.job_id]);
        pending_callbacks_.erase(job.job_id);
        return JS_ThrowInternalError(ctx, "worker %d job queue full", worker_id);
    }

    if (was_idle)
        uv_ref((uv_handle_t *)&async_); // work in flight — keep loop alive

    return JS_UNDEFINED;
}

void InferenceEngine::on_token_ready(uv_async_t *handle) {
    InferenceEngine *self = (InferenceEngine *)handle->data;
    JSContext *ctx = self->js_ctx_;

    for (auto &worker : self->workers_) {
        TokenResult result;
        while (worker->result_queue.pop(result)) {
            auto it = self->pending_callbacks_.find(result.job_id);
            if (it == self->pending_callbacks_.end()) continue;

            JSValue cb = it->second;

            if (result.error) {
                JSValue err = JS_NewString(ctx, result.token);
                JSValue args[2] = {err, JS_NULL};
                JSValue ret = JS_Call(ctx, cb, JS_UNDEFINED, 2, args);
                JS_FreeValue(ctx, ret);
                JS_FreeValue(ctx, err);
            } else {
                JSValue token = JS_NewString(ctx, result.token);
                JSValue args[2] = {JS_NULL, token};
                JSValue ret = JS_Call(ctx, cb, JS_UNDEFINED, 2, args);
                JS_FreeValue(ctx, ret);
                JS_FreeValue(ctx, token);
            }

            if (result.is_done) {
                JS_FreeValue(ctx, cb);
                self->pending_callbacks_.erase(it);
                if (self->pending_callbacks_.empty())
                    uv_unref((uv_handle_t *)&self->async_); // all done — idle again
            }
        }
    }
}
