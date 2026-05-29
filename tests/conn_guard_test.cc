#include "framework.h"
#include "conn_guard.h"

// acquire() zeroes the slot, so all fields start at zero/false/nullptr
static void acquired_slot_is_zeroed() {
    MemPool<ConnGuard> pool(4, "test-guards");
    ConnGuard *g = pool.acquire();
    REQUIRE(g != nullptr);
    CHECK(!g->alive);
    CHECK_EQ(g->ref_count, 0);
    CHECK(g->pool == nullptr);
    pool.release(g);
}

// acquire()/release() increment and decrement ref_count correctly.
// When ref_count reaches 0, the guard returns itself to the pool.
static void refcount_lifecycle() {
    MemPool<ConnGuard> pool(4, "test-guards");
    ConnGuard *g = pool.acquire();
    g->pool = &pool;

    g->acquire();
    CHECK_EQ(g->ref_count, 1);
    g->acquire();
    CHECK_EQ(g->ref_count, 2);

    g->release();               // ref_count → 1, still alive
    CHECK_EQ(g->ref_count, 1);
    CHECK_EQ(pool.in_use_count(), 1u);

    g->release();               // ref_count → 0 → pool.release(this); g is poisoned
    CHECK_EQ(pool.in_use_count(), 0u);
}

static void alive_flag_is_independent() {
    MemPool<ConnGuard> pool(4, "test-guards");
    ConnGuard *g = pool.acquire();
    g->pool = &pool;
    g->acquire();

    g->alive = true;
    CHECK(g->alive);
    g->alive = false;
    CHECK(!g->alive);

    g->release();
    CHECK_EQ(pool.in_use_count(), 0u);
}

static void last_release_returns_slot_to_pool() {
    MemPool<ConnGuard> pool(2, "test-guards");
    ConnGuard *g = pool.acquire();
    g->pool = &pool;
    CHECK_EQ(pool.in_use_count(), 1u);

    g->acquire();
    g->release();               // refcount 0 → pool.release(this)
    CHECK_EQ(pool.in_use_count(), 0u);
}

// Two guards from the same pool are independently ref-counted.
static void multiple_guards_are_independent() {
    MemPool<ConnGuard> pool(4, "test-guards");
    ConnGuard *g1 = pool.acquire();
    ConnGuard *g2 = pool.acquire();
    g1->pool = &pool;
    g2->pool = &pool;

    g1->acquire();
    g2->acquire();
    g2->acquire();
    CHECK_EQ(pool.in_use_count(), 2u);

    g1->release();              // g1 refcount 0 → back to pool
    CHECK_EQ(pool.in_use_count(), 1u);

    g2->release();              // g2 refcount 2 → 1
    CHECK_EQ(pool.in_use_count(), 1u);

    g2->release();              // g2 refcount 0 → back to pool
    CHECK_EQ(pool.in_use_count(), 0u);
}

int main() {
    RUN(acquired_slot_is_zeroed);
    RUN(refcount_lifecycle);
    RUN(alive_flag_is_independent);
    RUN(last_release_returns_slot_to_pool);
    RUN(multiple_guards_are_independent);
    CAPY_TEST_MAIN();
}
