
#include <iostream>
#include <cassert>
#include <vector>
#include <chrono>
#include "mem_pool.h"
#include <iomanip>
using namespace std::chrono;

// 400 + 8 + 2 = 582 BYTES
struct TestStruct {
    char mock_socket[200];
    char mock_handle[200];
    char buf[8];
    bool fd;
};

// ─── POOL BENCH ────────────────────────────────────────────────────────────
void bench_pool(size_t pool_size, size_t num_ops) {
    MemPool<TestStruct> pool(pool_size);
    std::vector<TestStruct*> live;
    live.reserve(pool_size);

    auto start = high_resolution_clock::now();

    for (size_t i = 0; i < num_ops; i++) {
        if (live.empty() || (!pool.is_exhausted() && i % 2 == 0)) {
            live.push_back(pool.acquire());
        } else {
            pool.release(live.back());
            live.pop_back();
        }
    }

    auto end = high_resolution_clock::now();
    auto ns  = duration_cast<nanoseconds>(end - start).count();

    std::cout << "  total:     " << ns << "ns\n";
    std::cout << "  ns/op:     " << ns / (double)num_ops << "ns\n";
     std::cout << "  ops/sec:   " << std::fixed << std::setprecision(0) << (num_ops / (double)ns) * 1e9 << "\n";
}

// ─── MALLOC BENCH ──────────────────────────────────────────────────────────
void bench_malloc(size_t num_ops) {
    std::vector<TestStruct*> live;
    live.reserve(10000);

    auto start = high_resolution_clock::now();

    for (size_t i = 0; i < num_ops; i++) {
        if (live.empty() || (live.size() < 10000 && i % 2 == 0)) {
            live.push_back((TestStruct*)malloc(sizeof(TestStruct)));
        } else {
            free(live.back());
            live.pop_back();
        }
    }

    auto end = high_resolution_clock::now();
    auto ns  = duration_cast<nanoseconds>(end - start).count();

    // free anything still live
    for (auto p : live) free(p);

    std::cout << "  total:     " << ns << "ns\n";
    std::cout << "  ns/op:     " << ns / (double)num_ops << "ns\n";
    std::cout << "  ops/sec:   " << std::fixed << std::setprecision(0) << (num_ops / (double)ns) * 1e9 << "\n";
}

// ─── FRAGMENTATION BENCH ───────────────────────────────────────────────────
// simulates realistic server pattern:
// alloc many, free every other, alloc again
// this is where malloc degrades and pool stays flat
void bench_pool_fragmented(size_t pool_size, size_t num_ops) {
    MemPool<TestStruct> pool(pool_size);
    std::vector<TestStruct*> live;
    live.reserve(pool_size);

    // phase 1: fill pool halfway
    for (size_t i = 0; i < pool_size / 2; i++) {
        live.push_back(pool.acquire( ));
    }

    auto start = high_resolution_clock::now();

    // phase 2: random acquire/release pattern (fragmented state)
    for (size_t i = 0; i < num_ops; i++) {
        if (i % 3 == 0 && !pool.is_exhausted()) {
            live.push_back(pool.acquire( ));
        } else if (!live.empty()) {
            // release from MIDDLE of live list — worst case for malloc
            size_t idx = live.size() / 2;
            pool.release(live[idx]);
            live.erase(live.begin() + idx);
        }
    }

    auto end = high_resolution_clock::now();
    auto ns  = duration_cast<nanoseconds>(end - start).count();

    std::cout << "  total:     " << ns << "ns\n";
    std::cout << "  ns/op:     " << ns / (double)num_ops << "ns\n";
    std::cout << "  ops/sec:   " << std::fixed << std::setprecision(0) << (num_ops / (double)ns) * 1e9 << "\n";
}

void bench_malloc_fragmented(size_t num_ops) {
    std::vector<TestStruct*> live;
    live.reserve(10000);

    // phase 1: fill halfway
    for (size_t i = 0; i < 5000; i++) {
        live.push_back((TestStruct*)malloc(sizeof(TestStruct)));
    }

    auto start = high_resolution_clock::now();

    // phase 2: same fragmented pattern
    for (size_t i = 0; i < num_ops; i++) {
        if (i % 3 == 0 && live.size() < 10000) {
            live.push_back((TestStruct*)malloc(sizeof(TestStruct)));
        } else if (!live.empty()) {
            size_t idx = live.size() / 2;
            free(live[idx]);
            live.erase(live.begin() + idx);
        }
    }

    auto end = high_resolution_clock::now();
    auto ns  = duration_cast<nanoseconds>(end - start).count();

    for (auto p : live) free(p);

    std::cout << "  total:     " << ns << "ns\n";
    std::cout << "  ns/op:     " << ns / (double)num_ops << "ns\n";
    std::cout << "  ops/sec:   " << (num_ops / (double)ns) * 1e9 << "\n";
}

int main() {
    const size_t OPS = 500000;

    std::cout << "\n=== SEQUENTIAL ACQUIRE/RELEASE (pool_size=10000) ===\n";
    std::cout << "[POOL]\n";
    bench_pool(10000, OPS);
    std::cout << "[MALLOC]\n";
    bench_malloc(OPS);

    std::cout << "\n=== FRAGMENTED PATTERN (realistic server load) ===\n";
    std::cout << "[POOL]\n";
    bench_pool_fragmented(10000, OPS);
    std::cout << "[MALLOC]\n";
    bench_malloc_fragmented(OPS);

    return 0;
}