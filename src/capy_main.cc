#include "builtins/builtins.h"
#include "mem_pool.h"
#include "quickjs.h"
#include "server.h"
#include "util.h"
#include "uv.h"
#include <iostream>

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

void tear_down_runtime_env(RuntimeContext *env, JSContext *ctx) {
  JSRuntime *rt = env->js_env;
  for (JSValue v : env->http_ctx->registerd_cb) {
    JS_FreeValue(ctx, v);
  }

  // Free Timer callbacks
  for (JSValue v : env->timer_ctx->registered_cb) {
    JS_FreeValue(ctx, v);
  }
  JS_RunGC(rt);
  JS_FreeContext(ctx);
  JS_FreeRuntime(rt);
  uv_loop_close(env->loop);
  uv_library_shutdown();
  env->allocator->verify_no_leaks();
  env->sock_alloc->verify_no_leaks();
  
  delete env->allocator;
  delete env->sock_alloc;
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

  // ################# Wire Libuv and QuickJS into RuntimeContext  ######## //
  HttpContext http_context;
  TimerContext timer_ctx;
  IdGenerator id_gen;
  RuntimeContext env;
  FSContext fs_ctx;

  env.loop = uv_default_loop();
  env.loop->data = &env;
  env.js_env = rt;
  env.js_ctx = ctx;
  env.http_ctx = &http_context;
  env.timer_ctx = &timer_ctx;
  env.id_generator = &id_gen;
  env.fs_ctx = &fs_ctx;
  env.allocator = new Allocator(); // Can easily swap with malloc/free or
                                   // straight up jemalloc
  env.sock_alloc = new Allocator(); // Allocator dedicated for websocket for debugging

  JS_SetRuntimeOpaque(rt, &env);

  // ################### Wire built-in libraries into JS engine
  // ######################### //
  setup_all_builtins(ctx);
  // ###################### Start JS isloate ################### //
  JSValue result = JS_Eval(ctx, code, len,
                           "<input>", // filename for errors
                           JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    dump_exception(ctx);
  }
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  
  free(code);
  tear_down_runtime_env(&env, ctx);
}
