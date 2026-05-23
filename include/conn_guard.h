#pragma once
#include "mem_pool.h"

struct ConnGuard {
    bool              alive;
    int               ref_count;
    MemPool<ConnGuard> *pool;

    void acquire() { ref_count++; }
    void release() { if (--ref_count == 0) pool->release(this); }
};
