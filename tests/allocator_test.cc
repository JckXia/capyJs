#include "allocator.h"
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <random>
#include <vector>

// TODO: Replace with your actual include
// #include "Allocator.hpp"

struct SmallObj {
  uint64_t id;
  uint32_t flags;
  uint16_t type;
  uint16_t pad;
}; // 16 bytes

struct MediumObj {
  char data[48];
  uint64_t timestamp;
  uint64_t checksum;
}; // 64 bytes

static_assert(sizeof(SmallObj) == 16, "SmallObj should be 16 bytes");
static_assert(sizeof(MediumObj) == 64, "MediumObj should be 64 bytes");

struct TestResult {
  size_t acquires;
  size_t releases;
  size_t failed_acquires;
  long duration_ms;
};

TestResult run_allocator_test(std::function<void *(size_t)> alloc_fn,
                              std::function<void(void *)> release_fn,
                              size_t num_ops = 500'000,
                              size_t max_live = 10'000, uint32_t seed = 42) {
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> op_dist(0, 1);
  std::uniform_int_distribution<int> type_dist(0, 1);

  std::vector<void *> live_ptrs;
  live_ptrs.reserve(max_live);

  size_t acquires = 0;
  size_t releases = 0;
  size_t failed_acquires = 0;

  auto start = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < num_ops; ++i) {
    bool do_acquire = op_dist(rng) == 0;

    if (live_ptrs.size() >= max_live)
      do_acquire = false;
    if (live_ptrs.empty())
      do_acquire = true;

    if (do_acquire) {
      size_t size =
          (type_dist(rng) == 0) ? sizeof(SmallObj) : sizeof(MediumObj);
      void *ptr = alloc_fn(size);

      if (ptr) {
        live_ptrs.push_back(ptr);
        acquires++;
      } else {
        failed_acquires++;
      }
    } else {
      std::uniform_int_distribution<size_t> idx_dist(0, live_ptrs.size() - 1);
      size_t idx = idx_dist(rng);

      release_fn(live_ptrs[idx]);

      live_ptrs[idx] = live_ptrs.back();
      live_ptrs.pop_back();
      releases++;
    }
  }

  // Cleanup remaining
  for (void *ptr : live_ptrs) {
    release_fn(ptr);
    releases++;
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
          .count();

  return {acquires, releases, failed_acquires, duration_ms};
}

void print_result(const std::string &name, const TestResult &r,
                  size_t num_ops) {
  std::cout << "=== " << name << " ===" << std::endl;
  std::cout << "Acquires:          " << r.acquires << std::endl;
  std::cout << "Releases:          " << r.releases << std::endl;
  std::cout << "Failed acquires:   " << r.failed_acquires << std::endl;
  std::cout << "Time:              " << r.duration_ms << " ms" << std::endl;
  std::cout << "Ops/sec:           "
            << (r.duration_ms > 0 ? (num_ops * 1000) / r.duration_ms : 0)
            << std::endl;
  std::cout << std::endl;
}

int main() {
  constexpr size_t NUM_OPS = 500'000;

  // Test malloc baseline
  auto malloc_result =
      run_allocator_test([](size_t size) { return malloc(size); },
                         [](void *ptr) { free(ptr); }, NUM_OPS);
  print_result("malloc", malloc_result, NUM_OPS);

  // TODO: Test your allocator
  Allocator *a = new Allocator();
  auto custom_result =
      run_allocator_test([a](size_t size) { return a->alloc(size); },
                         [a](void *ptr) { a->release(ptr); }, NUM_OPS);

  print_result("Allocator", custom_result, NUM_OPS);
  a->verify_no_leaks();
  delete a;

  return 0;
}