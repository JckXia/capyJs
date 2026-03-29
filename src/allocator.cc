#include "allocator.h"
#include <iostream>
Allocator::Allocator() {}

void* Allocator::alloc(size_t size) {
    return nullptr;
}

void Allocator::release(void *data) {
    std::cout<<sizeof(data) << std::endl;
}