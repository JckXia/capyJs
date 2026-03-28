#pragma once
#include <vector>
#include <map>
#include "quickjs.h"
#include "uv.h"

struct TimerContext {
    std::vector<JSValue> registered_cb;
    std::map<int, uv_timer_t*> registered_interval_cb;
};