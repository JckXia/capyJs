#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
// NOTE: This mem pool implementation is NOT threadsafe
//  -> It's sufficient for use cases like ours, where the server is a single
//  threaded program relying on
//     kernel level I/O notification
//  -> For other type of work (i.e uv threadpool) we'd need a
//  ThreadSafeMemoryPool
template <typename T> class MemPool {
public:
  explicit MemPool(size_t capacity);
  ~MemPool();

  T *acquire();
  void release(T *ptr);

  void dump_state() const;
  void dump_raw_state() const;

  size_t in_use_count() const;
  size_t capacity() const;

  void verify_no_leaks() const;

  bool is_exhausted() const;

private:
  struct Slot {
    T data;
    size_t next;
    bool in_use;
    Slot() : next(SIZE_MAX), in_use(false) {}
    friend std::ostream &operator<<(std::ostream &os, const Slot &s) {
      os << "next=" << s.next << " " << "in_use=" << s.in_use;
      return os;
    }
  };
  Slot *slots_;
  Slot *free_list_head;

  size_t capacity_;
  size_t in_use_count_;
};

template <typename T>
MemPool<T>::MemPool(size_t capacity) : capacity_(capacity), in_use_count_(0) {
  slots_ = new Slot[capacity]();
  for (int i = 0; i < capacity; i++) {
    if (i < capacity - 1) {
      slots_[i].next = i + 1;
    }
  }

  free_list_head = &slots_[0];
}

template <typename T> void MemPool<T>::verify_no_leaks() const {
  bool err_found = false;
  for (int i = 0; i < capacity_; i++) {
    if (slots_[i].in_use == true) {
      std::cout << "[ERROR] slot " << i << "  " << slots_[i]
                << " Has not been free'd! " << std::endl;

      err_found = true;
    }
  }
  if (!err_found) {
    std::cout << "[SUCCESS] No leak found. All memory freed \n";
  } else {
    dump_raw_state();
  }
}

template <typename T> T *MemPool<T>::acquire() {
  if (in_use_count_ == capacity_) {
    // Memory pool is exhausted
    return nullptr;
  }

  Slot *retHead = free_list_head;
  // Mark the current head as in-use
  free_list_head->in_use = true;

  // Get the current head's id and next node index
  size_t id = free_list_head - slots_;
  size_t next_node_idx = free_list_head->next;

  // set current head's next to -1, reattach prev to tail
  free_list_head->next = SIZE_MAX;
  free_list_head = &slots_[next_node_idx];
  memset(&retHead->data, 0, sizeof(T));
  in_use_count_ += 1;
  return &retHead->data;
}

template <typename T> void MemPool<T>::release(T *ptr) {
  assert(ptr != nullptr && "ERROR attempting to free an nullptr");
  Slot *slot = reinterpret_cast<Slot *>(ptr);
  assert(slot->in_use && "Double free detected!");
  if (slot == free_list_head) {
    slot->in_use = false;
    in_use_count_ -= 1;
    memset(&slot->data, 0xAD, sizeof(T));
    return;
  }

  memset(&slot->data, 0xAD, sizeof(T));
  slot->next = free_list_head - slots_;
  free_list_head = slot;
  slot->in_use = false;
  in_use_count_ -= 1;
}

template <typename T> void MemPool<T>::dump_state() const {
  std::cout << "Max Capacity: " << capacity_
            << " total in use: " << in_use_count_ << std::endl;
  std::cout << "Free List View " << std::endl;
  Slot *wh = free_list_head;
  size_t totalCapacity = capacity_;
  while (totalCapacity) {
    std::cout << *wh << std::endl;
    if (wh->next == SIZE_MAX) {
      break;
    }
    wh = &slots_[wh->next];
    totalCapacity -= 1;
  }
}

template <typename T> void MemPool<T>::dump_raw_state() const {
  std::cout << "Max Capacity: " << capacity_
            << " total in use: " << in_use_count_ << std::endl;
  std::cout << "Free List Head " << *free_list_head << std::endl;
  std::cout << "Data Dump" << std::endl;
  for (int i = 0; i < capacity_; i++) {
    std::cout << slots_[i] << std::endl;
  }
}

template <typename T> size_t MemPool<T>::in_use_count() const {
  return in_use_count_;
}

template <typename T> size_t MemPool<T>::capacity() const { return capacity_; }

template <typename T> bool MemPool<T>::is_exhausted() const {
  return in_use_count_ == capacity_;
}

template <typename T> MemPool<T>::~MemPool() { delete[] slots_; }