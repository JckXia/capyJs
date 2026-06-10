#pragma once
#include "i_manager.h"
#include "workers/inference_worker.h"
#include "llama.h"
#include "quickjs.h"
#include "uv.h"
#include <map>
#include <string>

class NativeInferenceManager : public IManager<InfJob, InfResult> {
public:
    NativeInferenceManager(JSContext *js_ctx, uv_loop_t *loop);

    NativeInferenceManager(const NativeInferenceManager &) = delete;
    NativeInferenceManager &operator=(const NativeInferenceManager &) = delete;

    // Load model weights under model_class. Call before spawn_workers.
    bool add_model(const char *model_class, const char *path);

    // Spin up count workers, dividing (nprocs-1) cores evenly among them.
    void spawn_workers(int count);

    // Workers call this (read-only after spawn_workers — no mutex needed).
    llama_model *get_model(const char *model_class);

    JSValue js_dispatch(JSContext *ctx, JSValue packet_val,
                        JSValue callback) override;
    JSValue js_get_progress(JSContext *ctx, JSValue job_id_val) override;
    JSValue js_close(JSContext *ctx) override;
    JSValue js_list_workers(JSContext *ctx);

    void on_result(const InfResult &r) override;

protected:
    IWorker<InfJob, InfResult> *pick_worker() override;

private:
    std::map<std::string, llama_model *> models_;
    std::map<uint64_t, JobStatus>        job_status_;
    JSContext  *js_ctx_;
    uv_loop_t  *loop_;
};
