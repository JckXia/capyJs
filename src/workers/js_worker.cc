#include "workers/js_worker.h"
#include <fstream>
#include <sstream>

JSWorker::JSWorker(int worker_id, IResultSink<JSResult> *sink,
                   JSContext *main_ctx, uv_loop_t *loop,
                   const std::string &worker_js_path)
    : worker_id_(worker_id), sink_(sink), main_ctx_(main_ctx),
      worker_js_path_(worker_js_path) {
    uv_async_init(loop, &doorbell_, on_result_ready);
    uv_unref((uv_handle_t *)&doorbell_);
    doorbell_.data = this;
    uv_thread_create(&thread_, thread_entry, this);
}

bool JSWorker::enqueue(JSJob job, JSValue callback) {
    if (!sub_queue_.enqueue({std::move(job), callback}))
        return false;
    if (in_flight_++ == 0)
        uv_ref((uv_handle_t *)&doorbell_);
    return true;
}

void JSWorker::stop() {
    sub_queue_.stop();
    uv_thread_join(&thread_);

    sub_queue_.drain([this](JSWorkItem &item) {
        JS_FreeValue(main_ctx_, item.callback);
    });

    JSResult r;
    while (result_queue_.pop(r))
        JS_FreeValue(main_ctx_, r.callback);

    uv_close((uv_handle_t *)&doorbell_, [](uv_handle_t *h) {
        delete static_cast<JSWorker *>(h->data);
    });
}

std::string JSWorker::load_file(const std::string &path) {
    std::ifstream f(path);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void JSWorker::thread_entry(void *arg) {
    static_cast<JSWorker *>(arg)->run();
}

void JSWorker::run() {
    std::string source = load_file(worker_js_path_);
    if (source.empty()) return;

    JSRuntime *rt  = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);

    JSValue eval_res = JS_Eval(ctx, source.c_str(), source.size(),
                               worker_js_path_.c_str(), JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(eval_res)) {
        JS_FreeValue(ctx, eval_res);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        return;
    }
    JS_FreeValue(ctx, eval_res);

    JSValue global     = JS_GetGlobalObject(ctx);
    JSValue process_fn = JS_GetPropertyStr(ctx, global, "process");
    JS_FreeValue(ctx, global);

    if (!JS_IsFunction(ctx, process_fn)) {
        JS_FreeValue(ctx, process_fn);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        return;
    }

    JSWorkItem item;
    while (sub_queue_.dequeue(item)) {
        JSResult result{};
        result.job_id   = item.job.job_id;
        result.callback = item.callback;

        JSValue input = JS_NewString(ctx, item.job.input_json.c_str());
        JSValue ret   = JS_Call(ctx, process_fn, JS_UNDEFINED, 1, &input);
        JS_FreeValue(ctx, input);

        if (JS_IsException(ret)) {
            JSValue    exc = JS_GetException(ctx);
            const char *msg = JS_ToCString(ctx, exc);
            result.error     = 1;
            result.error_msg = msg ? msg : "unknown error";
            JS_FreeCString(ctx, msg);
            JS_FreeValue(ctx, exc);
        } else {
            const char *out    = JS_ToCString(ctx, ret);
            result.output_json = out ? out : "";
            JS_FreeCString(ctx, out);
        }
        JS_FreeValue(ctx, ret);

        while (!result_queue_.push(result))
            ;

        uv_async_send(&doorbell_);
    }

    JS_FreeValue(ctx, process_fn);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

void JSWorker::on_result_ready(uv_async_t *handle) {
    auto *self = static_cast<JSWorker *>(handle->data);

    JSResult r;
    while (self->result_queue_.pop(r)) {
        self->sink_->on_result(r);
        if (--self->in_flight_ == 0)
            uv_unref((uv_handle_t *)handle);
    }
}
