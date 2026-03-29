#include <cstddef>
class Allocator {
    public:
        Allocator();
        void* alloc(size_t size);
        void release(void * data);
};