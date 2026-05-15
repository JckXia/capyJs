#pragma once
#include <cstddef>
#include <cstring>

// Fixed-capacity text ring buffer used as the per-session context window.
// The worker appends incoming prompt text; on overflow the oldest bytes are
// silently discarded (ring semantics). Phase 0: only push_str is wired up —
// llama.cpp will consume the underlying data in Phase 1.
struct RingBuffer {
    static constexpr size_t CAPACITY = 4096;

    char   data[CAPACITY] = {};
    size_t head = 0; // write position
    size_t len  = 0; // bytes currently stored

    void push_str(const char *s, size_t n) {
        for (size_t i = 0; i < n; i++) {
            data[head] = s[i];
            head = (head + 1) % CAPACITY;
            if (len < CAPACITY) len++;
        }
    }

    void push_str(const char *s) { push_str(s, strlen(s)); }

    void clear() { head = 0; len = 0; }
};
