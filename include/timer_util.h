#pragma once
#include <cstddef>
#include <random>

class IdGenerator {
public:
    IdGenerator()
        : rng(std::random_device{}()), dist(1000000, 9999999) {}

    int generate();

private:
    std::mt19937 rng;
    std::uniform_int_distribution<int> dist;
};