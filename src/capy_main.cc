#include "mem_pool.h"
#include "server.h"
#include "uv.h"
#include <iostream>

// Equivalent of Node's built-in modules
void init_http_ctx(HttpContext &ctx) {
  // ctx.emergency_handles = new MemPool<uv_tcp_t>
  ctx.connection_pool = new MemPool<ClientState>(10000); // C10K configuration
  ctx.read_buffer_pool = new MemPool<ReadBuffer>(200);
}

void on_signal(uv_signal_t *handle, int signum) {
  uv_signal_stop(handle);
  uv_close((uv_handle_t *)handle, NULL);
  uv_stop(uv_default_loop());
}

int main() {

  // ######################################## Plumbing around libuv and QuickJS
  // #####//
  HttpContext http_context;
  RuntimeContext env;
  MemPool<uv_tcp_t> emergency_handles(16);

  init_http_ctx(http_context);
  http_context.emergency_handles = &emergency_handles;

  env.loop = uv_default_loop();
  env.loop->data = &env;
  env.http_ctx = &http_context;

  uv_signal_t sig;
  uv_signal_init(uv_default_loop(), &sig);
  uv_signal_start(&sig, on_signal, SIGINT);
  // ######################################## Plumbing around libuv and QuickJS
  // #####//
  Server s(9091, &env);
  s.registerFuncHandler(
      "GET", "/hello", [](RequestObject &req, ResponseObject &res) {
        const char *msg = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/plain\r\n"
                          "Content-Length: 24\r\n"
                          "Connection: keep-alive\r\n"
                          "\r\n"
                          "hall from capyJS Server\n";

        res.response = msg;
        res.response_len = strlen(msg); // Can be abstracted away really.
      });
  s.run();

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  uv_loop_close(uv_default_loop());
  uv_library_shutdown();
}
