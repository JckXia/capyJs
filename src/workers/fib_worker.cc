#include "workers/fib_worker.h"

static uint64_t compute_fib(uint64_t n) {
    if (n <= 1) return n;
    return compute_fib(n - 1) + compute_fib(n - 2);
}

FibonacciWorker::FibonacciWorker(int worker_id, IResultSink<FibResult> *sink,
                                  JSContext *js_ctx, uv_loop_t *loop)
    : worker_id_(worker_id), sink_(sink), js_ctx_(js_ctx) {
    uv_async_init(loop, &doorbell_, on_result_ready);
    uv_unref((uv_handle_t *)&doorbell_);
    doorbell_.data = this;
    uv_thread_create(&thread_, thread_entry, this);
}

bool FibonacciWorker::enqueue(FibJob job, JSValue callback) {
    if (!sub_queue_.enqueue({job, callback}))
        return false;

    if (in_flight_++ == 0)
        uv_ref((uv_handle_t *)&doorbell_);

    return true;
}

void FibonacciWorker::stop() {
    sub_queue_.stop();
    uv_thread_join(&thread_);

    // Thread is gone — drain both queues on the main thread.
    sub_queue_.drain([this](WorkItem &item) {
        JS_FreeValue(js_ctx_, item.callback);
    });

    FibResult r;
    while (result_queue_.pop(r))
        JS_FreeValue(js_ctx_, r.callback);

    uv_close((uv_handle_t *)&doorbell_, [](uv_handle_t *h) {
        delete static_cast<FibonacciWorker *>(h->data);
    });
}

void FibonacciWorker::thread_entry(void *arg) {
    static_cast<FibonacciWorker *>(arg)->run();
}

void FibonacciWorker::run() {
    WorkItem item;
    while (sub_queue_.dequeue(item)) {
        FibResult result{};
        result.job_id   = item.job.job_id;
        result.result   = compute_fib(item.job.value);
        result.error    = 0;
        result.callback = item.callback; // carry as opaque bytes

        while (!result_queue_.push(result))
            ; // brief spin — result queue full, yield to consumer

        uv_async_send(&doorbell_);
    }
}

void FibonacciWorker::on_result_ready(uv_async_t *handle) {
    auto *self = static_cast<FibonacciWorker *>(handle->data);

    FibResult r;
    while (self->result_queue_.pop(r)) {
        self->sink_->on_result(r);

        if (--self->in_flight_ == 0)
            uv_unref((uv_handle_t *)handle);
    }
}
