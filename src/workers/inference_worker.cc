#include "workers/inference_worker.h"
#include "inference_engine.h"
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <unordered_map>

InferenceWorker::InferenceWorker(int worker_id, int core_start, int core_count,
                                  NativeInferenceManager *manager,
                                  JSContext *js_ctx, uv_loop_t *loop)
    : worker_id_(worker_id), core_start_(core_start), core_count_(core_count),
      manager_(manager), js_ctx_(js_ctx) {
    uv_async_init(loop, &doorbell_, on_result_ready);
    uv_unref((uv_handle_t *)&doorbell_);
    doorbell_.data = this;
    uv_thread_create(&thread_, thread_entry, this);
}

bool InferenceWorker::enqueue(InfJob job, JSValue callback) {
    if (!sub_queue_.enqueue({job, callback}))
        return false;

    if (in_flight_++ == 0)
        uv_ref((uv_handle_t *)&doorbell_);

    return true;
}

void InferenceWorker::stop() {
    sub_queue_.stop();
    uv_thread_join(&thread_);

    // Drain submission queue — free callbacks for jobs that never started.
    sub_queue_.drain([this](InfWorkItem &item) {
        JS_FreeValue(js_ctx_, item.callback);
    });

    // Drain result queue — free callbacks for jobs cut off mid-stream.
    // Multiple results share the same JSValue bits; free exactly once per job.
    std::unordered_map<uint64_t, JSValue> mid_stream;
    InfResult r;
    while (result_queue_.pop(r)) {
        if (r.is_done) {
            JS_FreeValue(js_ctx_, r.callback);
            mid_stream.erase(r.job_id);
        } else {
            mid_stream[r.job_id] = r.callback;
        }
    }
    for (auto &[id, cb] : mid_stream)
        JS_FreeValue(js_ctx_, cb);

    uv_close((uv_handle_t *)&doorbell_, [](uv_handle_t *h) {
        delete static_cast<InferenceWorker *>(h->data);
    });
}

void InferenceWorker::thread_entry(void *arg) {
    static_cast<InferenceWorker *>(arg)->run();
}

void InferenceWorker::run() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int i = 0; i < core_count_; i++)
        CPU_SET(core_start_ + i, &cpuset);

    if (pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) != 0)
        fprintf(stderr, "[worker %d] failed to pin to cores %d-%d\n",
                worker_id_, core_start_, core_start_ + core_count_ - 1);
    else
        fprintf(stderr, "[worker %d] pinned to cores %d-%d\n",
                worker_id_, core_start_, core_start_ + core_count_ - 1);

    InfWorkItem item;
    while (sub_queue_.dequeue(item))
        run_inference(item);
}

llama_context *InferenceWorker::get_or_create_ctx(const char *model_class,
                                                    llama_sampler *&sampler_out) {
    auto it = ctxs_.find(model_class);
    if (it != ctxs_.end()) {
        sampler_out = samplers_[model_class];
        return it->second;
    }

    llama_model *model = manager_->get_model(model_class);
    if (!model) {
        sampler_out = nullptr;
        return nullptr;
    }

    llama_context_params cp = llama_context_default_params();
    cp.n_ctx         = 2048;
    cp.n_batch       = 512;
    cp.n_threads     = core_count_;
    cp.n_threads_batch = core_count_;

    llama_context *ctx = llama_init_from_model(model, cp);
    if (!ctx) {
        fprintf(stderr, "[worker %d] failed to create context for '%s'\n",
                worker_id_, model_class);
        sampler_out = nullptr;
        return nullptr;
    }

    llama_sampler *sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(sampler, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.95f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    ctxs_[model_class]     = ctx;
    samplers_[model_class] = sampler;
    sampler_out = sampler;

    fprintf(stderr, "[worker %d] created context for '%s'\n", worker_id_, model_class);
    return ctx;
}

void InferenceWorker::send_error(uint64_t job_id, JSValue callback, const char *msg) {
    InfResult r{};
    r.job_id   = job_id;
    r.is_done  = true;
    r.error    = 1;
    r.callback = callback;
    snprintf(r.token, sizeof(r.token), "%s", msg);
    while (!result_queue_.push(r))
        ;
    uv_async_send(&doorbell_);
}

void InferenceWorker::run_inference(const InfWorkItem &item) {
    const InfJob &job = item.job;
    JSValue       cb  = item.callback;

    fprintf(stderr, "[worker %d] model=%s prompt=%.80s\n",
            worker_id_, job.model_class, job.prompt);

    llama_sampler *sampler = nullptr;
    llama_context *llm_ctx = get_or_create_ctx(job.model_class, sampler);
    if (!llm_ctx || !sampler) {
        send_error(job.job_id, cb, "model not available");
        return;
    }

    llama_model        *model = manager_->get_model(job.model_class);
    const llama_vocab  *vocab = llama_model_get_vocab(model);

    int n_ctx_max = (int)llama_n_ctx(llm_ctx);
    std::vector<llama_token> tokens(n_ctx_max);

    int n_tokens = llama_tokenize(vocab, job.prompt, strlen(job.prompt),
                                  tokens.data(), n_ctx_max,
                                  /*add_special=*/true, /*parse_special=*/false);
    if (n_tokens < 0) {
        send_error(job.job_id, cb, "tokenize failed");
        return;
    }
    tokens.resize(n_tokens);

    llama_memory_clear(llama_get_memory(llm_ctx), /*data=*/true);
    llama_sampler_reset(sampler);

    llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);
    if (llama_decode(llm_ctx, batch) != 0) {
        send_error(job.job_id, cb, "prompt eval failed");
        return;
    }

    bool sent_done = false;
    for (int i = 0; i < TOKEN_GEN_MAX; i++) {
        llama_token token_id = llama_sampler_sample(sampler, llm_ctx, -1);
        llama_sampler_accept(sampler, token_id);

        bool is_eog  = llama_vocab_is_eog(vocab, token_id);
        bool is_last = is_eog || (i == TOKEN_GEN_MAX - 1);

        char piece[256] = {};
        if (!is_eog) {
            int n = llama_token_to_piece(vocab, token_id, piece, sizeof(piece) - 1,
                                          /*lstrip=*/0, /*special=*/false);
            if (n > 0) piece[n] = '\0';
        }

        InfResult result{};
        result.job_id   = job.job_id;
        result.is_done  = is_last;
        result.error    = 0;
        result.callback = cb;
        snprintf(result.token, sizeof(result.token), "%s", piece);

        while (!result_queue_.push(result))
            ;
        uv_async_send(&doorbell_);
        sent_done = is_last;

        if (is_last) break;

        llama_batch next = llama_batch_get_one(&token_id, 1);
        if (llama_decode(llm_ctx, next) != 0) break;
    }

    if (!sent_done) {
        InfResult done{};
        done.job_id   = job.job_id;
        done.is_done  = true;
        done.error    = 0;
        done.callback = cb;
        while (!result_queue_.push(done))
            ;
        uv_async_send(&doorbell_);
    }
}

void InferenceWorker::on_result_ready(uv_async_t *handle) {
    auto *self = static_cast<InferenceWorker *>(handle->data);

    InfResult r;
    while (self->result_queue_.pop(r)) {
        self->manager_->on_result(r);

        if (r.is_done && --self->in_flight_ == 0)
            uv_unref((uv_handle_t *)handle);
    }
}
