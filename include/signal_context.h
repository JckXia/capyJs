#pragma once
#include "uv.h"
#include <map>

struct SigEntry {
    uv_signal_t handle;
    int refcount = 0;
    bool active = false;
};

struct SignalContext {
    std::map<int, SigEntry> signals;

    void register_daemon(uv_loop_t *loop, int signum);
    void deregister_daemon(int signum);
};
