#include "framework.h"
#include "work_queue.h"
#include <thread>

// Single-thread roundtrip: enqueue posts to semaphore so dequeue returns
// immediately without blocking.
static void single_thread_enqueue_dequeue() {
    WorkQueue<int, 4> q;
    CHECK(q.enqueue(77));
    int val = 0;
    CHECK(q.dequeue(val));
    CHECK_EQ(val, 77);
    q.stop();
}

static void stop_causes_dequeue_false() {
    WorkQueue<int, 4> q;
    q.stop();
    int val = -1;
    // stop() sets stopped_=true before posting; dequeue sees it and returns false
    CHECK(!q.dequeue(val));
}

// Items enqueued before stop() stay in the SPSC ring. drain() recovers them
// directly (no semaphore involvement) after the consumer thread is done.
static void drain_recovers_enqueued_items() {
    WorkQueue<int, 16> q;
    for (int i = 0; i < 5; i++) CHECK(q.enqueue(i));
    q.stop();
    int count = 0;
    q.drain([&](int) { count++; });
    CHECK_EQ(count, 5);
}

// Producer enqueues N items then signals stop. Consumer dequeues live items
// until it sees the stop signal; any items that raced past dequeue are
// recovered by drain(). Together they must sum to N.
static void threaded_producer_consumer() {
    WorkQueue<int, 64> q;
    constexpr int N = 30;
    int received_live  = 0;
    int received_drain = 0;

    std::thread producer([&] {
        for (int i = 0; i < N; i++) {
            while (!q.enqueue(i)) {}
        }
        q.stop();
    });

    int val;
    while (q.dequeue(val)) received_live++;
    producer.join();
    q.drain([&](int) { received_drain++; });

    CHECK_EQ(received_live + received_drain, N);
}

int main() {
    RUN(single_thread_enqueue_dequeue);
    RUN(stop_causes_dequeue_false);
    RUN(drain_recovers_enqueued_items);
    RUN(threaded_producer_consumer);
    CAPY_TEST_MAIN();
}
