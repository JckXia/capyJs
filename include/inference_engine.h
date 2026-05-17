#pragma once
#include "inference_worker.h"
#include "quickjs.h"
#include "uv.h"
#include "llama.h"
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

class InferenceEngine {
public:
    explicit InferenceEngine(JSContext *js_ctx, uv_loop_t *loop);

    InferenceEngine(const InferenceEngine &) = delete;
    InferenceEngine &operator=(const InferenceEngine &) = delete;

    // Called by the JS finalizer: stops all workers then frees model weights.
    // Caller must uv_close(async_handle(), ...) to finish teardown.
    void shutdown();

    // Load model weights and register under model_class. Call before spawn_workers.
    bool add_model(const char *model_class, const char *path);

    // Spin up count workers pinned to cores 1..count. Call after all add_model calls.
    void spawn_workers(int count);

    // Workers call this (read-only after spawn_workers; no mutex needed).
    llama_model *get_model(const char *model_class);

    JSValue js_list_workers(JSContext *ctx);
    JSValue js_do_inference(JSContext *ctx, JSValue model_class,
                            JSValue packet, JSValue callback);

    static void on_token_ready(uv_async_t *handle);
    uv_async_t *async_handle() { return &async_; }

private:
    std::map<std::string, llama_model *>          models_;
    std::vector<std::unique_ptr<InferenceWorker>> workers_;
    std::map<uint64_t, JSValue>                   pending_callbacks_;

    int        next_worker_rr_{0};
    uv_async_t async_;
    JSContext *js_ctx_;
    uint64_t   next_job_id_{0};
};
