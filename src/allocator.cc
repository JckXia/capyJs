#include "allocator.h"
#include "types.h"
#include <cstddef>
#include <iostream>

// alignas ensures header + 1 returns a pointer satisfying the strictest
// alignment required by any type we store (8 or 16 bytes depending on ABI).
// Without this, every returned pointer was 2 bytes past an 8-byte boundary,
// causing misaligned access faults on ARM.
struct alignas(alignof(std::max_align_t)) BlockHeader {
  uint8_t bin_index;
  uint8_t type_hint;
};

Allocator::Allocator() {
  this->b_8 = new MemPool<Block8>(8192);
  this->b_16 = new MemPool<Block16>(8192);
  this->b_32 = new MemPool<Block32>(4096);
  this->b_64 = new MemPool<Block64>(4096);
  this->b_128 = new MemPool<Block128>(2048);
  this->b_256 = new MemPool<Block256>(2048);
  this->b_512 = new MemPool<Block512>(2048);
  this->b_1024 = new MemPool<Block1024>(512);
}

Allocator::~Allocator() {
  delete this->b_8;
  delete this->b_16;
  delete this->b_32;
  delete this->b_64;
  delete this->b_128;
  delete this->b_256;
  delete this->b_512;
  delete this->b_1024;
}
void Allocator::verify_no_leaks() {
  size_t counts[64] = {}; // count per type_id, zero initialized

  auto scan = [&](auto *pool) {
    for (size_t i = 0; i < pool->capacity(); i++) {
      if (pool->slots_[i].in_use) {
        BlockHeader *h = (BlockHeader *)&pool->slots_[i].data;
        uint8_t type_id = h->type_hint;
        counts[type_id]++;
      }
    }
  };

  scan(b_8);
  scan(b_16);
  scan(b_32);
  scan(b_64);
  scan(b_128);
  scan(b_256);
  scan(b_512);
  scan(b_1024);

  bool any_leaked = false;
  for (int i = 0; i < 64; i++) {
    if (counts[i] > 0) {
      any_leaked = true;
      std::cout << "[LEAK] " << counts[i] << "x " << TYPE_NAMES[i] << "\n";
    }
  }

  if (!any_leaked)
    std::cout << "No leaks!\n";
}

size_t get_bin_index(size_t size) {
  if (size <= 8)
    return 0;
  // ceil(log2(size)) - 3
  return 64 - __builtin_clzl(size - 1) - 3;
}

void *Allocator::alloc_helper(size_t bin_idx) {
  if (bin_idx == 0)
    return b_8->acquire();
  if (bin_idx == 1)
    return b_16->acquire();
  if (bin_idx == 2)
    return b_32->acquire();
  if (bin_idx == 3)
    return b_64->acquire();
  if (bin_idx == 4)
    return b_128->acquire();
  if (bin_idx == 5)
    return b_256->acquire();
  if (bin_idx == 6)
    return b_512->acquire();
  if (bin_idx == 7)
    return b_1024->acquire();
  return nullptr;
}
void *Allocator::alloc(size_t size) {
  uint8_t bin_idx = get_bin_index(size + sizeof(BlockHeader));
  void *block = alloc_helper(bin_idx);
  if (block == nullptr) {
    return nullptr;
  }
  BlockHeader *header = static_cast<BlockHeader *>(block);
  header->bin_index = bin_idx;
  header->type_hint = AllocType::TYPE_UNKNOWN;

  return header + 1;
}

void *Allocator::alloc(size_t size, uint8_t type) {
  uint8_t bin_idx = get_bin_index(size + sizeof(BlockHeader));
  void *block = alloc_helper(bin_idx);
  if (block == nullptr) {
    return nullptr;
  }
  BlockHeader *header = static_cast<BlockHeader *>(block);
  header->bin_index = bin_idx;
  header->type_hint = type;
  return header + 1;
}

void Allocator::release(void *data) {
  BlockHeader *header = static_cast<BlockHeader *>(data) - 1;

  uint8_t bin_idx = header->bin_index;
  if (bin_idx == 0)
    return b_8->release((Block8 *)header);
  if (bin_idx == 1)
    return b_16->release((Block16 *)header);
  if (bin_idx == 2)
    return b_32->release((Block32 *)header);
  if (bin_idx == 3)
    return b_64->release((Block64 *)header);
  if (bin_idx == 4)
    return b_128->release((Block128 *)header);
  if (bin_idx == 5)
    return b_256->release((Block256 *)header);
  if (bin_idx == 6)
    return b_512->release((Block512 *)header);
  if (bin_idx == 7)
    return b_1024->release((Block1024 *)header);
}