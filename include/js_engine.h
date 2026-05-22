#pragma once
#include "i_manager.h"
#include "workers/js_worker.h"
#include "quickjs.h"
#include "uv.h"
#include <cstdint>
#include <map>
#include <string>

class NativeJSWorkerManager : public IManager<JSJob, JSResult> {
public:
    NativeJSWorkerManager(JSContext *js_ctx, uv_loop_t *loop,
                          int worker_size, const std::string &worker_js_path);

    NativeJSWorkerManager(const NativeJSWorkerManager &) = delete;
    NativeJSWorkerManager &operator=(const NativeJSWorkerManager &) = delete;

    JSValue js_dispatch(JSContext *ctx, JSValue input_val,
                        JSValue callback) override;
    JSValue js_get_progress(JSContext *ctx, JSValue job_id_val) override;
    JSValue js_close(JSContext *ctx) override;
    void    on_result(const JSResult &r) override;

protected:
    IWorker<JSJob, JSResult> *pick_worker() override;

private:
    std::map<uint64_t, JobStatus> job_status_;
    JSContext  *js_ctx_;
    uv_loop_t  *loop_;
    std::string worker_js_path_;
};
