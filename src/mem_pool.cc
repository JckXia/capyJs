#include "mem_pool.h"

template<typename T>
MemPool<T>::MemPool(size_t capacity): capacity_(capacity), in_use_count_(0) {
    slots_ = new Slot[capacity]();
    for(int i=0;i<capacity;i++){
        slots_[i].id = i;
        if (i  > 0) {
            slots_[i].prev = i - 1;
        }
        if (i < capacity -1) {
            slots_[i].next = i + 1;
        }
    }
    
    free_list_head = &slots_[0];
    free_list_tail = &slots_[capacity - 1];
}

 

template<typename T>
T* MemPool<T>::acquire() {
    if(in_use_count_ == capacity_) {
        // Memory pool is exhausted
        return nullptr;
    }

    // We have an problem!! This should never happen
    // in_use_count drifted from available free_list
    if (free_list_head->in_use == true) {
        return nullptr;
    }

    Slot* retHead = free_list_head;
    // Mark the current head as in-use
    free_list_head->in_use = true;
    
    // Get the current head's id and next node index
    size_t id = free_list_head->id;
    size_t next_node_idx = free_list_head->next;
    
    // set current head's next to -1, reattach prev to tail
    free_list_head->next = SIZE_MAX;
    free_list_head->prev = free_list_tail->id;
    free_list_tail->next = id;

    // Reset head and tail.
    free_list_tail = free_list_head;
    free_list_head = &slots_[next_node_idx];
    free_list_head->prev = SIZE_MAX;

    // Increment in use count by 1
    in_use_count_ += 1;
    return &retHead->data;
}

template<typename T>
void MemPool<T>::release(T* ptr) {
    if(ptr == nullptr) {
        return;
    }
    // size_t id = ptr->id;
    // Slot* object = slots_[id];
    Slot* slot = reinterpret_cast<Slot*>(
        reinterpret_cast<char*>(ptr) - offsetof(Slot, data)
    );

    assert(slot->in_use && "Double free detected!");
    if(slot == free_list_head) {
        slot->in_use = false;
        return;
    }

    if(slot->next != SIZE_MAX) {
        Slot* prevSlot = &slots_[slot->prev];
        Slot* afterSlot = &slots_[slot->next];
        prevSlot->next = afterSlot->id;
        afterSlot->prev = prevSlot->id;
    } else {
        // Will be the tail:
        Slot * prevSlot = &slots_[slot->prev];
        free_list_tail = prevSlot;
        prevSlot->next = SIZE_MAX;
    }

    // // Operation #2: append it to head of fre list
    slot->prev = SIZE_MAX;
    free_list_head->prev = slot->id;
    slot->next = free_list_head->id;
    slot->in_use = false;
    free_list_head = slot;
    in_use_count_ -= 1;
}

template<typename T>
void MemPool<T>::dump_state() const{
    std::cout<<"Max Capacity: "<< capacity_ <<" total in use: "<< in_use_count_<<std::endl;
    std::cout<<"Free List View " << std::endl;
    Slot* wh = free_list_head;
    size_t totalCapacity = capacity_;
    while (totalCapacity) {
        std::cout<< *wh << std::endl;
        if (wh->next == SIZE_MAX) {
            break;
        }
        wh = &slots_[wh->next]; 
        totalCapacity -= 1;
    }
}

template<typename T>
size_t MemPool<T>::in_use_count() const {
    return in_use_count_;
}

template<typename T>
size_t MemPool<T>::capacity()  const{
    return capacity_;
}

template<typename T>
bool MemPool<T>::is_exhausted() const {
    return in_use_count_ == capacity_;
}


template<typename T>
MemPool<T>::~MemPool() {
    delete[] slots_;
}

//TODO: Remove this after going into production.
// Also we should add some test suites!
struct Point {
    int x, y;
};

using namespace std;
int main() {
    std::cout<<"Hello World" << std::endl;
 
    MemPool<Point>* pool = new MemPool<Point>(5);
    Point * obj =  pool->acquire();
    pool->release(obj);

    auto v = pool->acquire();
    pool->release(v);
    // pool->acquire();
    // pool->acquire();
    // pool->acquire();
    // pool->acquire();
    pool->dump_state();
 
 
    delete pool;
}