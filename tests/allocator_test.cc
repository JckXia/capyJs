
#include <iostream>
#include "allocator.h"
// #include <cassert>
// #include <vector>
// #include <chrono>
// #include "mem_pool.h"
// #include <iomanip>
// using namespace std::chrono;
struct BigAssData {
    int v;
    int f;
    int g;
    int h;
    int l;

};
int main() {
    Allocator a;

    // int * v = (int *) a.allooc(sizeof(int));
    // int * r = (int *) mallc(sizeof(int));
    // a.release(r);

    BigAssData * d = (BigAssData*) malloc(sizeof(BigAssData));
    a.release(d);
    std::cout<<sizeof(BigAssData) << std::endl;
}