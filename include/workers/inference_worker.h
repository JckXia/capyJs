#pragma once
#include "i_worker.h"
#include "work_queue.h"
#include "spsc_queue.h"
#include "llama.h"
#include "quickjs.h"
#include "uv.h"
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>

class NativeInferenceManager;

struct InfJob {
    uint64_t job_id;
    char     model_class[32];
    char     session_id[128];
    char     client_id[64];
    char     prompt[2048];
};

struct InfWorkItem {
    InfJob  job;
    JSValue callback; // opaque on worker thread — never dereferenced there
};

struct InfResult {
    uint64_t job_id;
    char     token[64];
    bool     is_done;
    int      error;
    JSValue  callback; // carried as opaque bytes, called on main thread only
};

class InferenceWorker : public IWorker<InfJob, InfResult> {
public:
    static constexpr size_t QUEUE_DEPTH        = 64;
    static constexpr size_t RESULT_QUEUE_DEPTH = 256;
    static constexpr int    TOKEN_GEN_MAX      = 512;

    InferenceWorker(int worker_id, int core_start, int core_count,
                    int n_ctx, int n_batch,
                    NativeInferenceManager *manager,
                    JSContext *js_ctx, uv_loop_t *loop);

    InferenceWorker(const InferenceWorker &) = delete;
    InferenceWorker &operator=(const InferenceWorker &) = delete;

    bool   enqueue(InfJob job, JSValue callback) override;
    void   stop() override;
    size_t queue_depth() const override { return sub_queue_.empty() ? 0 : 1; }
    int    worker_id() const override { return worker_id_; }
    int    core_start() const { return core_start_; }
    int    core_count() const { return core_count_; }

    static void on_result_ready(uv_async_t *handle);

private:
    static void thread_entry(void *arg);
    void run();
    void run_inference(const InfWorkItem &item);
    void send_error(uint64_t job_id, JSValue callback, const char *msg);

    // Returns cached context for model_class, creating it on first use.
    llama_context *get_or_create_ctx(const char *model_class,
                                      llama_sampler *&sampler_out);

    WorkQueue<InfWorkItem, QUEUE_DEPTH>      sub_queue_;
    SPSCQueue<InfResult, RESULT_QUEUE_DEPTH> result_queue_;

    uv_async_t  doorbell_;
    uv_thread_t thread_;
    int         worker_id_;
    int         core_start_;
    int         core_count_;
    int         n_ctx_;
    int         n_batch_;
    int         in_flight_{0}; // main-thread only; tracks jobs (not tokens)

    std::map<std::string, llama_context *> ctxs_;
    std::map<std::string, llama_sampler *> samplers_;

    NativeInferenceManager *manager_;
    JSContext              *js_ctx_;
};
