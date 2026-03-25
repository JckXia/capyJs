# pragma once
#include "http_context.h"

struct RuntimeContext {
    uv_loop_t* loop;
    HttpContext * http_ctx;
};