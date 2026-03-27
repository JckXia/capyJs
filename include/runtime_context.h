#pragma once
#include "http_context.h"
#include "quickjs.h"
struct RuntimeContext {
  uv_loop_t *loop;
  JSRuntime * js_env;
  HttpContext *http_ctx;
};