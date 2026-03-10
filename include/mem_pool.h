#include <cstddef>
#include <iostream>
#include <cstdint>
#include <cassert>
// NOTE: This mem pool implementation is NOT threadsafe
//  -> It's sufficient for use cases like ours, where the server is a single threaded program relying on
//     kernel level I/O notification
//  -> For other type of work (i.e uv threadpool) we'd need a ThreadSafeMemoryPool
template<typename T>
class MemPool {
    public:
        explicit MemPool(size_t capacity);
        ~MemPool();

        T* acquire();
        void release(T *ptr);

        void dump_state() const;

        size_t in_use_count() const;
        size_t capacity() const;

        bool is_exhausted() const;
    private:
        struct Slot {
            size_t id;  // TODO: Can be remoed to save on memory
            T data;
            size_t prev;
            size_t next;
            bool in_use;
            Slot(): prev(SIZE_MAX), next(SIZE_MAX), in_use(false) {}
            friend std::ostream& operator<<(std::ostream& os, const Slot& s) {
                os << "Slot[" << s.id << "] "
                << "prev=" << s.prev << " "
                << "next=" << s.next << " "
                << "in_use=" << s.in_use;
                return os;
            }
      
        };     
        Slot* slots_;
        Slot* free_list_head;
        Slot* free_list_tail;

        size_t capacity_;
        size_t in_use_count_;
};