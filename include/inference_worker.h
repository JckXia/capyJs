#pragma once
#include "spsc_queue.h"
#include "llama.h"
#include "uv.h"
#include <atomic>
#include <cstdint>
#include <map>
#include <string>

class InferenceEngine;

struct JobItem {
    uint64_t job_id;
    char     model_class[32];
    char     session_id[128];
    char     prompt[2048];
};

struct TokenResult {
    uint64_t job_id;
    char     token[64];
    bool     is_done;
    int      error;
};

struct WorkerStats {
    int worker_id;
    int core_start;
    int core_count;
    int queued_depth;
};

class InferenceWorker {
public:
    static constexpr size_t JOB_QUEUE_DEPTH    = 64;
    static constexpr size_t RESULT_QUEUE_DEPTH = 256;
    static constexpr int    TOKEN_GEN_MAX      = 512;

    InferenceWorker(int worker_id, int core_start, int core_count, InferenceEngine *parent);
    ~InferenceWorker();

    InferenceWorker(const InferenceWorker &) = delete;
    InferenceWorker &operator=(const InferenceWorker &) = delete;

    bool enqueue_job(const JobItem &job);
    void start();
    void stop();

    WorkerStats stats() const;

    SPSCQueue<TokenResult, RESULT_QUEUE_DEPTH> result_queue;

private:
    static void thread_entry(void *arg);
    void run();
    void run_inference(const JobItem &job);
    void send_error(uint64_t job_id, const char *msg);

    // Returns cached context or creates one for model_class on first use.
    llama_context *get_or_create_ctx(const char *model_class, llama_sampler *&sampler_out);

    SPSCQueue<JobItem, JOB_QUEUE_DEPTH> job_queue_;

    int              worker_id_;
    int              core_start_;
    int              core_count_;
    InferenceEngine *parent_;

    // One llama_context + sampler per model class, created lazily on the worker thread.
    std::map<std::string, llama_context *> ctxs_;
    std::map<std::string, llama_sampler *> samplers_;

    uv_thread_t       thread_;
    std::atomic<bool> running_{false};
};
