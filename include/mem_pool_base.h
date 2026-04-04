class MemPoolBase {
protected:
    void* m_buffer;
    size_t m_blockSize;
    size_t m_numBlocks;
    // Free list, tracking, etc.
    struct Slot {
        void * data;
        
    };

    Slot *slots_;
    Slot* free_list_head;
    size_t capacity_;
    size_t in_use_count;

public:
    MemPoolBase(size_t blockSize, size_t numBlocks)
        : m_blockSize(blockSize), m_numBlocks(numBlocks) {
        m_buffer = ::operator new(blockSize * numBlocks);
        // Initialize free list, etc.
    }
    
    virtual ~MemPoolBase() {
        ::operator delete(m_buffer);
    }

    void* allocate() {
        // Same logic for all T - just returns void*
    }

    void deallocate(void* ptr) {
        // Same logic for all T
    }
};
