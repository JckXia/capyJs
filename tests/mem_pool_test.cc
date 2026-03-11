
#include <iostream>
#include <cassert>
#include <vector>
#include "mem_pool.h"
using namespace std;

struct MemPoolTest {
    int x;
    int y;
    MemPoolTest(): x(0), y(0) {}
};

struct Op {
    std::string type;   // "ACQUIRE" or "RELEASE"
    int*        ptr;
    size_t      in_use_count;
};

void dump_op_log(std::vector<Op> &op_log) {
    std::cout << "\n=== OPERATION LOG ===\n";
    for (size_t i = 0; i < op_log.size(); i++) {
        std::cout << "[" << i << "] "
                  << op_log[i].type
                  << " ptr=" << op_log[i].ptr
                  << " in_use=" << op_log[i].in_use_count
                  << "\n";
    }
    std::cout << "=====================\n";
}

template<typename T, typename Predicate>
bool __allocSuccessful(T* obj, Predicate check) {
    if(obj == nullptr) {
        return false;
    }
    return check(obj);
}


bool __structAllocSuccessful(MemPoolTest* s) {
    return __allocSuccessful(s , [](MemPoolTest *p) {
        return p->x == 0 && p->y == 0;
    });
}

// ##################################### Above are helper classess ###################### //

void test_basic_acquire() {
    MemPool<int> pool(5);
    int* a = pool.acquire();
    assert(a != nullptr);
    assert(pool.in_use_count() == 1);
    assert(pool.is_exhausted() == false);
    std::cout<<"PASS: test_basic_acquire"<<std::endl;
}

void test_pool_exhaustion() {
    MemPool<int> pool(3);
    pool.acquire();
    pool.acquire();
    pool.acquire();
    assert(pool.in_use_count() == 3);
    assert(pool.is_exhausted() == true);
    std::cout<<"PASS: test_pool_exhaustion"<<std::endl;
}

void test_release_rejoins_free_list() {
    MemPool<MemPoolTest> pool(3);
    auto a = pool.acquire();
    assert(pool.in_use_count() == 1);
    assert(pool.is_exhausted() == false);
    pool.release(a);
    assert(pool.in_use_count() == 0);
    std::cout<<"PASS: test_release_rejoins_free_list"<<std::endl;
}

void test_reclaim_successful() {
    MemPool<MemPoolTest> pool(3);
    auto a = pool.acquire();
    a->x = 1;
    a->y = 2;
    
    pool.release(a);
    assert(pool.in_use_count() == 0);

    a = pool.acquire();
    auto b = pool.acquire();
    auto c = pool.acquire();
    assert(pool.is_exhausted() == true);
    assert(a->x == 0);
    assert(b->x == 0);
    assert(c->x == 0);
    std::cout<<"PASS: test_reclaim_successful"<<std::endl;
}

void test_out_of_order_alloc_dealloc() {

    MemPool<MemPoolTest> pool(5);
    auto a = pool.acquire();
    auto b = pool.acquire();
    assert(__structAllocSuccessful(a));
    assert(__structAllocSuccessful(b));
    assert(pool.in_use_count() == 2);
    b->x = 20;
    a->x = 15;

    // Free b:
    pool.release(b);
    assert(pool.in_use_count() == 1);
    b = pool.acquire();
    assert(pool.in_use_count() == 2);
 
    std::cout<<"PASS: test_out_of_order_alloc_dealloc"<<std::endl;
}

void test_random_alloc_dealloc() {
    const size_t POOL_SIZE = 7;
    MemPool<int> pool(POOL_SIZE);
    std::vector<Op> op_log;
    std::vector<int*> live_ptrs;  // currently acquired pointers
    srand(42);                     // fixed seed = reproducible

    const int NUM_OPS = 500000;

    for (int i = 0; i < NUM_OPS; i++) {
        bool should_acquire;

        if (live_ptrs.empty()) {
            should_acquire = true;   // nothing to release, must acquire
        } else if (pool.is_exhausted()) {
            should_acquire = false;  // pool full, must release
        } else {
            should_acquire = (rand() % 2 == 0);  // random
        }

        if (should_acquire) {
            int* ptr = pool.acquire();

            // sanity checks
            if (ptr == nullptr) {
                std::cout << "ERROR: acquire() returned nullptr but pool not exhausted!\n";
                dump_op_log(op_log);
                assert(false);
            }

            // check poison from previous use — every byte should NOT be 0xAD
            // (fresh slot is zeroed, reused slot was poisoned then given back)
            *ptr = i;  // write known value

            op_log.push_back({"ACQUIRE", ptr, pool.in_use_count()});
            live_ptrs.push_back(ptr);

       //     std::cout << "ACQUIRE ptr=" << ptr << " val=" << *ptr
          //            << " in_use=" << pool.in_use_count() << "\n";

        } else {
            // pick random live pointer to release
            size_t idx = rand() % live_ptrs.size();
            int*   ptr = live_ptrs[idx];

            // verify value is still what we wrote (no corruption from another acquire)
        //    std::cout << "RELEASE ptr=" << ptr << " val=" << *ptr
        //              << " in_use=" << pool.in_use_count() << "\n";

            pool.release(ptr);
            op_log.push_back({"RELEASE", ptr, pool.in_use_count()});

            // verify poisoning
            unsigned char* bytes = reinterpret_cast<unsigned char*>(ptr);
            for (size_t b = 0; b < sizeof(int); b++) {
                if (bytes[b] != 0xAD) {
                    std::cout << "ERROR: byte[" << b << "] = 0x"
                              << std::hex << (int)bytes[b]
                              << " expected 0xAD after release!\n";
                    dump_op_log(op_log);
                    assert(false);
                }
            }

            // remove from live_ptrs
            live_ptrs.erase(live_ptrs.begin() + idx);
        }

        // invariant check every op
        if (pool.in_use_count() != live_ptrs.size()) {
            std::cout << "ERROR: in_use_count=" << pool.in_use_count()
                      << " but live_ptrs.size()=" << live_ptrs.size() << "\n";
            dump_op_log(op_log);
            assert(false);
        }
    }

    // cleanup — release everything still live
    for (int* ptr : live_ptrs) {
        pool.release(ptr);
    }

    // final invariant
    if (pool.in_use_count() != 0) {
        std::cout << "ERROR: pool not empty after releasing everything!\n";
        pool.dump_raw_state();
        dump_op_log(op_log);
        assert(false);
    }

    std::cout << "\nPASS: test_random_alloc_dealloc (" << NUM_OPS << " ops)\n";
}


 

int main() {
    std::cout<<"Execute mem_pool tests:\n";
    test_basic_acquire();
    test_pool_exhaustion();
    test_release_rejoins_free_list();
    test_reclaim_successful();
    test_out_of_order_alloc_dealloc();
    test_random_alloc_dealloc();
}