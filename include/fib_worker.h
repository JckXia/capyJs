#pragma once
#include "i_worker.h"
#include "work_queue.h"
#include "spsc_queue.h"
#include "quickjs.h"
#include "uv.h"
#include <cstdint>

class NativeFibonacciManager;

struct FibJob {
    uint64_t job_id;
    uint64_t value;
};

struct WorkItem {
    FibJob  job;
    JSValue callback; // opaque on worker thread — never dereferenced there
};

struct FibResult {
    uint64_t job_id;
    uint64_t result;
    int      error;
    JSValue  callback; // carried as opaque bytes, called on main thread only
};

class FibonacciWorker : public IWorker<FibJob, FibResult> {
public:
    static constexpr size_t QUEUE_DEPTH        = 64;
    static constexpr size_t RESULT_QUEUE_DEPTH = 64;

    FibonacciWorker(int worker_id, NativeFibonacciManager *manager,
                    JSContext *js_ctx, uv_loop_t *loop);

    FibonacciWorker(const FibonacciWorker &) = delete;
    FibonacciWorker &operator=(const FibonacciWorker &) = delete;

    bool   enqueue(FibJob job, JSValue callback) override;
    void   stop() override;
    size_t queue_depth() const override { return sub_queue_.empty() ? 0 : 1; }
    int    worker_id() const override { return worker_id_; }

    static void on_result_ready(uv_async_t *handle);

private:
    static void thread_entry(void *arg);
    void run();

    WorkQueue<WorkItem, QUEUE_DEPTH>         sub_queue_;
    SPSCQueue<FibResult, RESULT_QUEUE_DEPTH> result_queue_;

    uv_async_t  doorbell_;
    uv_thread_t thread_;
    int         worker_id_;
    int         in_flight_{0}; // main-thread only

    NativeFibonacciManager *manager_;
    JSContext              *js_ctx_;
};
