#pragma once
#include "http_context.h"
#include "quickjs.h"
#include "timer_context.h"
#include "server.h"
#include "timer_util.h"
#include "fs_context.h"
class Server;

struct RuntimeContext {
  ~RuntimeContext() {
    delete server_pools;
  }
  IdGenerator *id_generator;
  uv_loop_t *loop;
  JSRuntime * js_env;
  HttpContext *http_ctx;
  TimerContext *timer_ctx;
  MemPool<Server> *server_pools;
  FSContext * fs_ctx;
};