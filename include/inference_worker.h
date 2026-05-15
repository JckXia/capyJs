#pragma once
#include "spsc_queue.h"
#include "ring_buffer.h"
#include "uv.h"
#include <atomic>
#include <cstdint>
#include <map>
#include <string>

class InferenceEngine;

struct JobItem {
    uint64_t job_id;
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
    int         worker_id;
    const char *model_class;
    int         queued_depth;
};

class InferenceWorker {
public:
    static constexpr size_t JOB_QUEUE_DEPTH    = 64;
    static constexpr size_t RESULT_QUEUE_DEPTH = 256;

    InferenceWorker(int worker_id, const char *model_class,
                    const char *model_path, InferenceEngine *parent);
    ~InferenceWorker();

    InferenceWorker(const InferenceWorker &) = delete;
    InferenceWorker &operator=(const InferenceWorker &) = delete;

    bool enqueue_job(const JobItem &job);
    void start();
    void stop();

    WorkerStats stats() const;
    const char *model_class() const { return model_class_; }

    // Consumed by InferenceEngine::on_token_ready on the main thread
    SPSCQueue<TokenResult, RESULT_QUEUE_DEPTH> result_queue;

private:
    static void thread_entry(void *arg);
    void run();
    void run_inference(const JobItem &job);

    SPSCQueue<JobItem, JOB_QUEUE_DEPTH> job_queue_;

    int              worker_id_;
    char             model_class_[32];
    char             model_path_[256];
    InferenceEngine *parent_;  // for uv_async_send after each token

    std::map<std::string, RingBuffer *> context_windows_;

    uv_thread_t       thread_;
    std::atomic<bool> running_{false};
};
