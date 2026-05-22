#pragma once
#include "spsc_queue.h"
#include "uv.h"
#include <atomic>
#include <cstddef>

// Lock-free work queue for single-producer / single-consumer use.
//
// Data transfer uses SPSCQueue (pure atomics, no locks on hot path).
// Blocking when empty uses a semaphore — the worker sleeps only when there
// is genuinely nothing to do; sem_post/sem_wait on a non-zero count is a
// single atomic op with no kernel involvement.
//
// N must be a power of 2 (SPSCQueue requirement). Usable capacity is N-1.
//
// Thread safety:
//   enqueue()  — producer thread only
//   dequeue()  — consumer thread only
//   stop()     — any thread (typically the producer signalling shutdown)
//   drain()    — call after consumer thread is joined (single-threaded)

template <typename T, size_t N>
class WorkQueue {
public:
    WorkQueue() {
        uv_sem_init(&sem_, 0);
    }

    ~WorkQueue() {
        uv_sem_destroy(&sem_);
    }

    WorkQueue(const WorkQueue &) = delete;
    WorkQueue &operator=(const WorkQueue &) = delete;

    // Returns false if full — caller handles backpressure.
    bool enqueue(const T &item) {
        if (!queue_.push(item)) return false;
        uv_sem_post(&sem_);
        return true;
    }

    // Blocks until an item is available or stop() has been called.
    // Returns false when stopped and no items remain — consumer should exit.
    bool dequeue(T &out) {
        uv_sem_wait(&sem_);
        if (stopped_.load(std::memory_order_acquire)) return false;
        return queue_.pop(out);
    }

    // Wake the blocked consumer so it can observe the stopped flag and exit.
    void stop() {
        stopped_.store(true, std::memory_order_release);
        uv_sem_post(&sem_);
    }

    // Walk and remove all remaining items, calling fn(item) on each.
    // Must be called only after the consumer thread has been joined.
    template <typename Fn>
    void drain(Fn &&fn) {
        T item;
        while (queue_.pop(item))
            fn(item);
    }

    bool empty() const { return queue_.empty(); }

private:
    SPSCQueue<T, N>   queue_;
    uv_sem_t          sem_;
    std::atomic<bool> stopped_{false};
};
