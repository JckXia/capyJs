#pragma once
#include "i_worker.h"
#include "i_result_sink.h"
#include "quickjs.h"
#include "uv.h"
#include <cstddef>
#include <cstdint>
#include <vector>

enum class JobStatus { PENDING, DONE, ERROR };

// IManager<TJob, TResult>
//
// Worker-aware pool and sole JS-layer entry point for a given work type.
// Inherits IResultSink<TResult> — workers hold a IResultSink* back-pointer
// and call on_result() without needing to know the full manager type.
// The manager has no uv_* handles; libuv lifecycle is entirely the workers'.

template <typename TJob, typename TResult>
class IManager : public IResultSink<TResult> {
public:
    virtual ~IManager() = default;

    virtual JSValue js_dispatch(JSContext *ctx, JSValue job_val,
                                JSValue callback) = 0;
    virtual JSValue js_get_progress(JSContext *ctx, JSValue job_id_val) = 0;
    virtual JSValue js_close(JSContext *ctx) = 0;

protected:
    virtual IWorker<TJob, TResult> *pick_worker() = 0;

    std::vector<IWorker<TJob, TResult> *> workers_;
    uint64_t next_job_id_{0};
    size_t   next_worker_idx_{0};
};
