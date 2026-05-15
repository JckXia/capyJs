#pragma once
#include "inference_worker.h"
#include "quickjs.h"
#include "uv.h"
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

struct ModelConfig {
    char model_class[32];
    char path[256];
};

class InferenceEngine {
public:
    explicit InferenceEngine(JSContext *js_ctx, uv_loop_t *loop);

    InferenceEngine(const InferenceEngine &) = delete;
    InferenceEngine &operator=(const InferenceEngine &) = delete;

    // Called by the JS finalizer: stops all workers (joins threads), then
    // the caller must uv_close(async_handle(), ...) to finish teardown.
    void shutdown();

    bool add_worker(const char *model_class, const char *path);

    // JS-facing methods (called on main thread)
    JSValue js_list_workers(JSContext *ctx);
    JSValue js_do_inference(JSContext *ctx, int worker_id,
                            JSValue packet, JSValue callback);

    // uv_async_t callback — fires on main thread when any worker signals.
    // Drains all result queues and dispatches stored JS callbacks.
    static void on_token_ready(uv_async_t *handle);

    uv_async_t *async_handle() { return &async_; }

private:
    std::vector<std::unique_ptr<InferenceWorker>> workers_;
    std::map<uint64_t, JSValue> pending_callbacks_;  // job_id → JS callback

    uv_async_t  async_;   // async_.data = this; workers call uv_async_send(&async_)
    JSContext  *js_ctx_;
    uint64_t    next_job_id_{0};
};
