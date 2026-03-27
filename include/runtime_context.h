#pragma once
#include "http_context.h"
#include "quickjs.h"
#include "server.h"
class Server;

struct RuntimeContext {
  ~RuntimeContext() {
    delete server_pools;
  }
  
  uv_loop_t *loop;
  JSRuntime * js_env;
  HttpContext *http_ctx;
  MemPool<Server> *server_pools;
};