#pragma once
#include <atomic>
#include <cstddef>

// Lock-free single-producer / single-consumer ring queue.
// Capacity must be a power of 2. Actual usable slots = N - 1.
// write_ is owned exclusively by the producer; read_ by the consumer.
template <typename T, size_t N>
class SPSCQueue {
    static_assert((N & (N - 1)) == 0, "N must be a power of 2");
    static_assert(N > 1, "N must be > 1");

public:
    SPSCQueue() = default;
    SPSCQueue(const SPSCQueue &) = delete;
    SPSCQueue &operator=(const SPSCQueue &) = delete;

    bool push(const T &item) {
        size_t w = write_.load(std::memory_order_relaxed);
        size_t next = (w + 1) & (N - 1);
        if (next == read_.load(std::memory_order_acquire))
            return false;
        buf_[w] = item;
        write_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T &out) {
        size_t r = read_.load(std::memory_order_relaxed);
        if (r == write_.load(std::memory_order_acquire))
            return false;
        out = buf_[r];
        read_.store((r + 1) & (N - 1), std::memory_order_release);
        return true;
    }

    bool empty() const {
        return read_.load(std::memory_order_acquire) ==
               write_.load(std::memory_order_acquire);
    }

private:
    T buf_[N];
    alignas(64) std::atomic<size_t> write_{0};
    alignas(64) std::atomic<size_t> read_{0};
};
