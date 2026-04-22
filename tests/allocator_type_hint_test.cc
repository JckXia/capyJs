#include "allocator.h"

#include "types.h"
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <random>
#include <vector>

void simple_test_for_leak_verification() {
  Allocator allocator;
  allocator.alloc(512, AllocType::TYPE_CLIENT_STATE);
  allocator.alloc(512, AllocType::TYPE_CLIENT_STATE);
  allocator.alloc(512, AllocType::TYPE_CLIENT_STATE);
  allocator.alloc(18, AllocType::TYPE_UV_TCP);
  allocator.alloc(512, AllocType::TYPE_WRITE_BUFFER);
  allocator.verify_no_leaks();
}

int main() {
  simple_test_for_leak_verification();

  return 0;
}