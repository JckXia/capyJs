#pragma once
#include "i_manager.h"
#include "fib_worker.h"
#include "quickjs.h"
#include "uv.h"
#include <cstdint>
#include <map>

enum class JobStatus { PENDING, DONE, ERROR };

class NativeFibonacciManager : public IManager<FibJob, FibResult> {
public:
    NativeFibonacciManager(JSContext *js_ctx, uv_loop_t *loop, int worker_size);

    NativeFibonacciManager(const NativeFibonacciManager &) = delete;
    NativeFibonacciManager &operator=(const NativeFibonacciManager &) = delete;

    JSValue js_dispatch(JSContext *ctx, JSValue value_val,
                        JSValue callback) override;
    JSValue js_get_progress(JSContext *ctx, JSValue job_id_val) override;
    JSValue js_close(JSContext *ctx) override;
    void    on_result(const FibResult &r) override;

protected:
    IWorker<FibJob, FibResult> *pick_worker() override;

private:
    std::map<uint64_t, JobStatus> job_status_;
    JSContext *js_ctx_;
    uv_loop_t *loop_;
};
