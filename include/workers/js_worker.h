#pragma once
#include "i_worker.h"
#include "i_result_sink.h"
#include "work_queue.h"
#include "spsc_queue.h"
#include "quickjs.h"
#include "uv.h"
#include <cstdint>
#include <string>

struct JSJob {
    uint64_t    job_id;
    std::string input_json;
};

struct JSWorkItem {
    JSJob   job;
    JSValue callback; // opaque on worker thread
};

struct JSResult {
    uint64_t    job_id;
    std::string output_json;
    int         error{0};
    std::string error_msg;
    JSValue     callback; // carried as opaque bytes, called on main thread only
};

class JSWorker : public IWorker<JSJob, JSResult> {
public:
    static constexpr size_t QUEUE_DEPTH        = 32;
    static constexpr size_t RESULT_QUEUE_DEPTH = 32;

    JSWorker(int worker_id, IResultSink<JSResult> *sink,
             JSContext *main_ctx, uv_loop_t *loop,
             const std::string &worker_js_path);

    JSWorker(const JSWorker &) = delete;
    JSWorker &operator=(const JSWorker &) = delete;

    bool   enqueue(JSJob job, JSValue callback) override;
    void   stop() override;
    size_t queue_depth() const override { return sub_queue_.empty() ? 0 : 1; }
    int    worker_id() const override { return worker_id_; }

    static void on_result_ready(uv_async_t *handle);

private:
    static void thread_entry(void *arg);
    void run(); // JSRuntime + JSContext live as locals here for the thread lifetime

    static std::string load_file(const std::string &path);

    WorkQueue<JSWorkItem, QUEUE_DEPTH>       sub_queue_;
    SPSCQueue<JSResult, RESULT_QUEUE_DEPTH>  result_queue_;

    uv_async_t  doorbell_;
    uv_thread_t thread_;
    int         worker_id_;
    int         in_flight_{0};

    IResultSink<JSResult> *sink_;
    JSContext             *main_ctx_;     // main thread only — JS_FreeValue in stop()
    std::string            worker_js_path_;
};
