#include "inference_engine.h"
#include <cstring>
#include <iostream>

InferenceEngine::InferenceEngine(JSContext *js_ctx, uv_loop_t *loop)
    : js_ctx_(js_ctx) {
    uv_async_init(loop, &async_, on_token_ready);
    uv_unref((uv_handle_t *)&async_); // idle — don't block loop exit
    async_.data = this;
}

void InferenceEngine::shutdown() {
    for (auto &w : workers_) w->stop();
}

bool InferenceEngine::add_worker(const char *model_class, const char *path) {
    int id = (int)workers_.size();
    workers_.push_back(
        std::make_unique<InferenceWorker>(id, model_class, path, this));
    workers_.back()->start();
    return true;
}

JSValue InferenceEngine::js_list_workers(JSContext *ctx) {
    JSValue arr = JS_NewArray(ctx);
    for (int i = 0; i < (int)workers_.size(); i++) {
        WorkerStats s = workers_[i]->stats();
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "worker_id",   JS_NewInt32(ctx, s.worker_id));
        JS_SetPropertyStr(ctx, obj, "model_class", JS_NewString(ctx, s.model_class));
        JS_SetPropertyStr(ctx, obj, "queued_work", JS_NewInt32(ctx, s.queued_depth));
        JS_SetPropertyUint32(ctx, arr, i, obj);
    }
    return arr;
}

JSValue InferenceEngine::js_do_inference(JSContext *ctx, int worker_id,
                                         JSValue packet, JSValue callback) {
    if (worker_id < 0 || worker_id >= (int)workers_.size())
        return JS_ThrowRangeError(ctx, "invalid worker_id %d", worker_id);

    if (!JS_IsFunction(ctx, callback))
        return JS_ThrowTypeError(ctx, "callback must be a function");

    JSValue sid_val    = JS_GetPropertyStr(ctx, packet, "session_id");
    JSValue prompt_val = JS_GetPropertyStr(ctx, packet, "prompt");

    const char *sid    = JS_ToCString(ctx, sid_val);
    const char *prompt = JS_ToCString(ctx, prompt_val);

    JobItem job{};
    job.job_id = next_job_id_++;
    snprintf(job.session_id, sizeof(job.session_id), "%s", sid    ? sid    : "default");
    snprintf(job.prompt,     sizeof(job.prompt),     "%s", prompt ? prompt : "");

    JS_FreeCString(ctx, sid);
    JS_FreeCString(ctx, prompt);
    JS_FreeValue(ctx, sid_val);
    JS_FreeValue(ctx, prompt_val);

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
                JSValue err = JS_NewString(ctx, "inference error");
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
