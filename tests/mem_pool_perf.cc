
#include <iostream>
#include <cassert>
#include <vector>
#include <chrono>
#include "mem_pool.h"
using namespace std;
struct MemPoolTest {
    int x;
    int y;
    MemPoolTest(): x(0), y(0) {}
};

using namespace std::chrono;

void bench_acquire_release(size_t pool_size, size_t num_ops) {
    MemPool<int> pool(pool_size);
    std::vector<int*> live;
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

    std::cout << "=== BENCH: pool_size=" << pool_size 
              << " ops=" << num_ops << " ===\n";
    std::cout << "total:      " << ns << "ns\n";
    std::cout << "ns per op:  " << ns / (double)num_ops << "ns\n";
    std::cout << "ops/sec:    " << (num_ops / (double)ns) * 1e9 << "\n";
}
int main() {
    bench_acquire_release(10,    100000);   // tiny pool
    bench_acquire_release(1000,  100000);   // medium pool  
    bench_acquire_release(10000, 500000);   // C10K pool
}