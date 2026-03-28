#include "timer_util.h"
#include <random>

int IdGenerator::generate() {
    return dist(this->rng);
}
