#pragma once
#include "http_context.h"
#include "quickjs.h"
#include "timer_context.h"
#include "server.h"
#include "timer_util.h"
#include "fs_context.h"
#include "signal_context.h"
struct RuntimeContext {

  IdGenerator *id_generator;
  uv_loop_t *loop;
  JSRuntime * js_env;
  JSContext* js_ctx;
  HttpContext *http_ctx;
  TimerContext *timer_ctx;
  FSContext * fs_ctx;
  SignalContext *signal_ctx;
};