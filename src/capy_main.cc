#include "mem_pool.h"
#include "quickjs.h"
#include "server.h"
#include "uv.h"
#include "builtins/builtins.h"
#include <iostream>

static char *read_file(const char *filename, size_t *out_len) {
  FILE *f = fopen(filename, "rb");
  if (!f) {
    fprintf(stderr, "Error: cannot open '%s'\n", filename);
    return nullptr;
  }

  fseek(f, 0, SEEK_END);
  size_t len = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *buf = (char *)malloc(len + 1);
  if (!buf) {
    fclose(f);
    return nullptr;
  }

  fread(buf, 1, len, f);
  buf[len] = '\0';
  fclose(f);

  if (out_len)
    *out_len = len;
  return buf;
}

// Equivalent of Node's built-in modules
void init_http_ctx(HttpContext &ctx) {
  ctx.emergency_handles = new MemPool<uv_tcp_t>(16);
  ctx.connection_pool = new MemPool<ClientState>(10000); // C10K configuration
  ctx.read_buffer_pool = new MemPool<ReadBuffer>(200);
}


 void dump_exception(JSContext* ctx) {
    JSValue exc = JS_GetException(ctx);
    
    const char* msg = JS_ToCString(ctx, exc);
    fprintf(stderr, "Exception: %s\n", msg ? msg : "(no message)");
    if (msg) JS_FreeCString(ctx, msg);
    
    JSValue stack = JS_GetPropertyStr(ctx, exc, "stack");
    if (!JS_IsUndefined(stack)) {
        const char* stack_str = JS_ToCString(ctx, stack);
        fprintf(stderr, "%s\n", stack_str);
        JS_FreeCString(ctx, stack_str);
    }
    JS_FreeValue(ctx, stack);
    JS_FreeValue(ctx, exc);
}
static JSClassID server_class_id;
static JSClassID response_class_id;

static JSValue response_send(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)  {
    ResponseObject* res = (ResponseObject*)JS_GetOpaque2(ctx, this_val, response_class_id);
    if (!res) return JS_EXCEPTION;
    
    const char* body = JS_ToCString(ctx, argv[0]);
    size_t len = strlen(body);
    
    if (len >= sizeof(res->response_buf)) {
        JS_FreeCString(ctx, body);
        return JS_ThrowRangeError(ctx, "Response too large");
    }
    
    memcpy(res->response_buf, body, len + 1);  // Copy into your buffer
    res->response_len = len;
    
    JS_FreeCString(ctx, body);  // Safe to free now
    return JS_UNDEFINED;
}

static JSClassDef response_class_def = {
    .class_name = "Response"
};   


void setup_http_response_class(JSContext *ctx) {
 
    JSRuntime *rt = JS_GetRuntime(ctx);
    JS_NewClassID(&response_class_id);
    JS_NewClass(rt, response_class_id, &response_class_def);
    JSValue res_proto = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, res_proto, "send",
        JS_NewCFunction(ctx, response_send, "send", 1));
    JS_SetClassProto(ctx, response_class_id, res_proto);
}

 

// ############### Server #################### //
static void server_finalizer(JSRuntime *rt, JSValue val) {
  Server *server = (Server *)JS_GetOpaque(val, server_class_id);
  RuntimeContext * env = (RuntimeContext*) JS_GetRuntimeOpaque(rt);
  env->server_pools->release(server);
}

struct JSClassDef server_class_def = {
    "Server",
    .finalizer = server_finalizer,
};

static JSValue register_get_url(JSContext *ctx, JSValueConst this_val, int argc,
                                JSValueConst *argv) {
  Server *server = (Server *)JS_GetOpaque2(ctx, this_val, server_class_id);
  if (!server)
    return JS_EXCEPTION;

  const char *uri = JS_ToCString(ctx, argv[0]);
  // JSValue callback = argv[1];
 RuntimeContext * env = (RuntimeContext*) JS_GetRuntimeOpaque(JS_GetRuntime(ctx)); 
 JSValue callback = JS_DupValue(ctx, argv[1]); 
  server->registerFuncHandler(
      "GET", uri, [ctx, callback](RequestObject &req, ResponseObject &res) {
        JSValue js_req = JS_NewObjectClass(ctx, request_class_id);
        JSValue js_res = JS_NewObjectClass(ctx, response_class_id);
        JS_SetOpaque(js_req, &req);
        JS_SetOpaque(js_res, &res);

        // call the JS callback
        JSValue args[] = {js_req, js_res};
        JS_Call(ctx, callback, JS_UNDEFINED, 2, args);

        JS_FreeValue(ctx, js_req);
        JS_FreeValue(ctx, js_res);
      });
  env->http_ctx->registerd_cb.push_back(callback);
  JS_FreeCString(ctx, uri);
  return JS_UNDEFINED;
}

static JSValue server_listen(JSContext* ctx, JSValueConst this_val,
                              int argc, JSValueConst* argv) {
    Server* server = (Server*)JS_GetOpaque2(ctx, this_val, server_class_id);
    if (!server) return JS_EXCEPTION;
    
    int32_t port;
    JS_ToInt32(ctx, &port, argv[0]);
    
    // Optional callback
    if (argc > 1 && JS_IsFunction(ctx, argv[1])) {
        JSValue ret = JS_Call(ctx, argv[1], JS_UNDEFINED, 0, nullptr);
        JS_FreeValue(ctx, ret);
    }
    
    server->run(); // TODO: refactor this
    return JS_UNDEFINED;
}

static JSValue server_constructor(JSContext* ctx, JSValueConst new_target,
                                   int argc, JSValueConst* argv) {

    JSValue obj = JS_NewObjectClass(ctx, server_class_id);
    if (JS_IsException(obj)) return obj;
    
    JSRuntime * rt = JS_GetRuntime(ctx);
    RuntimeContext * env = (RuntimeContext*)JS_GetRuntimeOpaque(rt);
    int32_t port;
    JS_ToInt32(ctx, &port, argv[0]);

    Server* server = env->server_pools->acquire();
    server->setPortNum(port);
    server->setEnv(env);
    
 
    JS_SetOpaque(obj, server);
 
    return obj;
}

void setup_server_class(JSContext * ctx) {
    JSRuntime* rt = JS_GetRuntime(ctx);
    JS_NewClassID(&server_class_id);
    JS_NewClass(rt, server_class_id, &server_class_def);
    JSValue server_proto = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, server_proto, "get",
        JS_NewCFunction(ctx, register_get_url, "get", 2));
    JS_SetPropertyStr(ctx, server_proto, "listen",
        JS_NewCFunction(ctx, server_listen, "listen", 2));
        JS_SetClassProto(ctx, server_class_id, server_proto);
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue ctor = JS_NewCFunction2(ctx, server_constructor, "Server", 0,
                                     JS_CFUNC_constructor, 0);

    JS_SetPropertyStr(ctx, global, "Server", ctor);
    JS_FreeValue(ctx, global);
}

 

void on_signal(uv_signal_t *handle, int signum) {
  uv_signal_stop(handle);
  uv_close((uv_handle_t *)handle, NULL);
  uv_stop(uv_default_loop());
}

static JSValue console_log(JSContext *ctx, JSValueConst this_val, int argc,
                           JSValueConst *argv) {
  // ... logging implementation ...
  for (int i = 0; i < argc; i++) {
    JSValue val = argv[i];

    if (JS_IsString(val)) {
      const char *str = JS_ToCString(ctx, val);
      printf("string: %s\n", str);
      JS_FreeCString(ctx, str);

    } else if (JS_IsNumber(val)) {
      double num;
      JS_ToFloat64(ctx, &num, val);
      printf("number: %f\n", num);

    } else if (JS_IsBool(val)) {
      int b = JS_ToBool(ctx, val);
      printf("bool: %s\n", b ? "true" : "false");

    } else if (JS_IsNull(val)) {
      printf("null\n");

    } else if (JS_IsUndefined(val)) {
      printf("undefined\n");

    } else if (JS_IsArray(ctx, val)) {
      printf("array (length: ???)\n");

    } else if (JS_IsFunction(ctx, val)) {
      printf("function\n");

    } else if (JS_IsObject(val)) {
      printf("object\n");

    } else {
      printf("unknown type\n");
    }
  }

  return JS_UNDEFINED;
}
 

void setup_console(JSContext *ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue console = JS_NewObject(ctx);
  JSValue func = JS_NewCFunction(ctx, console_log, "log", 1);
  JS_SetPropertyStr(ctx, console, "log", func);

  JS_SetPropertyStr(ctx, global, "console", console);
  JS_FreeValue(ctx, global);
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
  init_http_ctx(http_context); // TODO: Might need to move httpContext to heap....Rasp Pi stack is pretty small...
  // ################# Wire Libuv and QuickJS into RuntimeContext  ######## //
  RuntimeContext env;
  env.loop = uv_default_loop();
  env.loop->data = &env;
  env.js_env = rt;
  env.http_ctx = &http_context;
  env.server_pools = new MemPool<Server>(3);
  JS_SetRuntimeOpaque(rt, &env);

  // ################### Wire built-in libraries into JS engine ######################### //
 // setup_console(ctx);
  setup_all_builtins(ctx);
 
  setup_http_response_class(ctx);
  setup_server_class(ctx);
  // ###################### Start JS isloate ################### //
  JSValue result = JS_Eval(ctx, code, len,
                           "<input>", // filename for errors
                           JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    dump_exception(ctx);
   }
 
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  uv_loop_close(uv_default_loop());


  free(code); // Well this is annoying.
  for(JSValue v : http_context.registerd_cb) {
    JS_FreeValue(ctx, v);
  }
 
  JS_FreeContext(ctx);
  JS_FreeRuntime(rt);
 
  uv_library_shutdown();
}
