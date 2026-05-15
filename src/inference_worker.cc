#include "inference_worker.h"
#include "inference_engine.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <unistd.h>

static const char *VOCAB[] = {
    "the", "quick", "brown", "fox", "jumps", "over", "lazy", "dog",
    "hello", "world", "how", "are", "you", "I", "am", "fine", "a",
    "an", "is", "was", "with", "and", "that", "this", "on", "in",
};
static constexpr int VOCAB_SIZE = sizeof(VOCAB) / sizeof(VOCAB[0]);
static constexpr int TOKEN_GEN_MAX = 20;
static constexpr int TOKEN_DELAY_US = 10000; // 10 ms simulated latency

InferenceWorker::InferenceWorker(int worker_id, const char *model_class,
                                 const char *model_path, InferenceEngine *parent)
    : worker_id_(worker_id), parent_(parent) {
    snprintf(model_class_, sizeof(model_class_), "%s", model_class);
    snprintf(model_path_,  sizeof(model_path_),  "%s", model_path);
}

InferenceWorker::~InferenceWorker() {
    for (auto &[id, rb] : context_windows_) delete rb;
}

bool InferenceWorker::enqueue_job(const JobItem &job) {
    return job_queue_.push(job);
}

void InferenceWorker::start() {
    running_.store(true, std::memory_order_release);
    uv_thread_create(&thread_, thread_entry, this);
}

void InferenceWorker::stop() {
    running_.store(false, std::memory_order_release);
    uv_thread_join(&thread_);
}

WorkerStats InferenceWorker::stats() const {
    // Approximate depth: difference between write and read cursors.
    // We expose this as a rough backpressure signal for the JS layer.
    return {worker_id_, model_class_, job_queue_.empty() ? 0 : 1};
}

void InferenceWorker::thread_entry(void *arg) {
    static_cast<InferenceWorker *>(arg)->run();
}

void InferenceWorker::run() {
    while (running_.load(std::memory_order_acquire)) {
        JobItem job;
        if (job_queue_.pop(job)) {
            run_inference(job);
        } else {
            usleep(1000); // 1 ms idle poll
        }
    }
}

void InferenceWorker::run_inference(const JobItem &job) {
    // Maintain context window for this session
    auto it = context_windows_.find(job.session_id);
    if (it == context_windows_.end()) {
        context_windows_[job.session_id] = new RingBuffer();
        it = context_windows_.find(job.session_id);
    }
    it->second->push_str(job.prompt);

    int n_tokens = 5 + (rand() % (TOKEN_GEN_MAX - 4));

    for (int i = 0; i < n_tokens; i++) {
        if (!running_.load(std::memory_order_acquire)) break;

        usleep(TOKEN_DELAY_US);

        TokenResult result{};
        result.job_id  = job.job_id;
        result.is_done = (i == n_tokens - 1);
        result.error   = 0;

        const char *word = VOCAB[rand() % VOCAB_SIZE];
        if (result.is_done) {
            snprintf(result.token, sizeof(result.token), "%s", word);
        } else {
            snprintf(result.token, sizeof(result.token), "%s ", word);
        }

        // Push token then wake the main thread
        while (!result_queue.push(result)) {
            usleep(500); // back-pressure: result queue full, yield
        }
        uv_async_send(parent_->async_handle());
    }
}
