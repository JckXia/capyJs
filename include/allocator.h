#include "buffer.h"
#include "mem_pool.h"
#include <cstddef>
// Bin in increments:
// 8, 16, 32, 64, 128, 256, 512, 1024, 2048
class Allocator {
public:
  explicit Allocator();
  ~Allocator();
  void *alloc(size_t size);
  void release(void *data);
  void verify_no_leaks();

private:
  void *alloc_helper(size_t size);
  MemPool<Block8> *b_8;
  MemPool<Block16> *b_16;
  MemPool<Block32> *b_32;
  MemPool<Block64> *b_64;
  MemPool<Block128> *b_128;
  MemPool<Block256> *b_256;
  MemPool<Block512> *b_512;
  MemPool<Block1024> *b_1024;
};