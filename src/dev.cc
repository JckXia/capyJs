#include <iostream>
#include "uv.h"
#include "server.h"

int main() {
    Server s(9091,"0.0.0.0");
    s.run();
}