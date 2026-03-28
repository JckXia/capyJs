#include "builtins/builtins.h"
#include "mem_pool.h"
#include "quickjs.h"
#include "server.h"
#include "uv.h"
#include "util.h"
#include <iostream>

// Equivalent of Node's built-in modules
void init_http_ctx(HttpContext &ctx) {
  ctx.emergency_handles = new MemPool<uv_tcp_t>(16);
  ctx.connection_pool = new MemPool<ClientState>(10000); // C10K configuration
  ctx.read_buffer_pool = new MemPool<ReadBuffer>(200);
}

void dump_exception(JSContext *ctx) {
  JSValue exc = JS_GetException(ctx);

  const char *msg = JS_ToCString(ctx, exc);
  fprintf(stderr, "Exception: %s\n", msg ? msg : "(no message)");
  if (msg)
    JS_FreeCString(ctx, msg);

  JSValue stack = JS_GetPropertyStr(ctx, exc, "stack");
  if (!JS_IsUndefined(stack)) {
    const char *stack_str = JS_ToCString(ctx, stack);
    fprintf(stderr, "%s\n", stack_str);
    JS_FreeCString(ctx, stack_str);
  }
  JS_FreeValue(ctx, stack);
  JS_FreeValue(ctx, exc);
}

void on_signal(uv_signal_t *handle, int signum) {
  uv_signal_stop(handle);
  uv_close((uv_handle_t *)handle, NULL);
  uv_stop(uv_default_loop());
}

int main(int argc, char **argv) {

  // #################### Read input file #############//
  if (argc < 2) {
    std::cerr << "provide a script! \n";
    return 1;
  }
  const char *filename = argv[1];
  size_t len;
  char *code = read_file(filename, &len);
  if (!code) {
    std::cerr << "File " << filename << " not found!" << std::endl;
    return 1;
  }

  // #################### Init Libuv signal trapper for handling graceful
  // shutdowns #############//
  uv_signal_t sig;
  uv_signal_init(uv_default_loop(), &sig);
  uv_signal_start(&sig, on_signal, SIGINT);
  // ############################################# //

  // ##### Init JS runtime ######## //
  JSRuntime *rt = JS_NewRuntime();
  JSContext *ctx = JS_NewContext(rt);

  // ################### Init Http context ########## //
  HttpContext http_context;
  init_http_ctx(http_context); // TODO: Might need to move httpContext to
                               // heap....Rasp Pi stack is pretty small...
  // ################# Wire Libuv and QuickJS into RuntimeContext  ######## //
  TimerContext timer_ctx;

  RuntimeContext env;
  env.loop = uv_default_loop();
  env.loop->data = &env;
  env.js_env = rt;
  env.http_ctx = &http_context;
  env.timer_ctx = &timer_ctx;
  env.server_pools = new MemPool<Server>(3);
  
  JS_SetRuntimeOpaque(rt, &env);

  // ################### Wire built-in libraries into JS engine
  // ######################### //
  // setup_console(ctx);
  setup_all_builtins(ctx);
  // ###################### Start JS isloate ################### //
  JSValue result = JS_Eval(ctx, code, len,
                           "<input>", // filename for errors
                           JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    dump_exception(ctx);
  }

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  

  free(code); // Well this is annoying.
  for (JSValue v : http_context.registerd_cb) {
    JS_FreeValue(ctx, v);
  }

  // Free Timer callbacks
  for (JSValue v : timer_ctx.registered_cb) {
    JS_FreeValue(ctx, v);
  }

  JS_FreeContext(ctx);
  JS_FreeRuntime(rt);
  uv_loop_close(uv_default_loop());
  uv_library_shutdown();
}
