#include "framework.h"
#include "spsc_queue.h"

static void push_pop_roundtrip() {
    SPSCQueue<int, 4> q;
    CHECK(q.push(42));
    int val = -1;
    CHECK(q.pop(val));
    CHECK_EQ(val, 42);
}

static void pop_on_empty_returns_false() {
    SPSCQueue<int, 4> q;
    int val;
    CHECK(!q.pop(val));
}

static void push_to_full_returns_false() {
    // N=4, ring sentinel wastes one slot → 3 usable
    SPSCQueue<int, 4> q;
    CHECK(q.push(1));
    CHECK(q.push(2));
    CHECK(q.push(3));
    CHECK(!q.push(4));
    int val;
    CHECK(q.pop(val)); CHECK_EQ(val, 1);
    CHECK(q.pop(val)); CHECK_EQ(val, 2);
    CHECK(q.pop(val)); CHECK_EQ(val, 3);
}

static void fifo_ordering() {
    SPSCQueue<int, 8> q;
    for (int i = 0; i < 5; i++) CHECK(q.push(i * 10));
    for (int i = 0; i < 5; i++) {
        int val = -1;
        CHECK(q.pop(val));
        CHECK_EQ(val, i * 10);
    }
}

static void empty_reflects_state() {
    SPSCQueue<int, 4> q;
    CHECK(q.empty());
    q.push(7);
    CHECK(!q.empty());
    int v;
    q.pop(v);
    CHECK(q.empty());
}

static void ring_wraps_correctly() {
    // push/pop cycles that cross the N boundary exercise the ring mask
    SPSCQueue<int, 4> q; // 3 usable slots
    for (int round = 0; round < 6; round++) {
        CHECK(q.push(100 + round));
        CHECK(q.push(200 + round));
        int v;
        CHECK(q.pop(v)); CHECK_EQ(v, 100 + round);
        CHECK(q.pop(v)); CHECK_EQ(v, 200 + round);
    }
}

int main() {
    RUN(push_pop_roundtrip);
    RUN(pop_on_empty_returns_false);
    RUN(push_to_full_returns_false);
    RUN(fifo_ordering);
    RUN(empty_reflects_state);
    RUN(ring_wraps_correctly);
    CAPY_TEST_MAIN();
}
