#include "builtins/server_js.h"
#include "builtins.h"
#include "quickjs.h"
#include "server.h"
static void server_finalizer(JSRuntime *rt, JSValue val) {
  Server *server = (Server *)JS_GetOpaque(val, server_class_id);
  RuntimeContext *env = (RuntimeContext *)JS_GetRuntimeOpaque(rt);
  env->server_pools->release(server);
}

struct JSClassDef server_class_def = {
    "Server",
    .finalizer = server_finalizer,
};
JSClassID server_class_id;

static JSValue register_get_url(JSContext *ctx, JSValueConst this_val, int argc,
                                JSValueConst *argv) {
  Server *server = (Server *)JS_GetOpaque2(ctx, this_val, server_class_id);
  if (!server)
    return JS_EXCEPTION;

  const char *uri = JS_ToCString(ctx, argv[0]);

  RuntimeContext *env =
      (RuntimeContext *)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
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

static JSValue server_listen(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
  Server *server = (Server *)JS_GetOpaque2(ctx, this_val, server_class_id);
  if (!server)
    return JS_EXCEPTION;

  int32_t port;
  JS_ToInt32(ctx, &port, argv[0]);
  server->setPortNum(port);
  // Optional callback
  if (argc > 1 && JS_IsFunction(ctx, argv[1])) {
    JSValue ret = JS_Call(ctx, argv[1], JS_UNDEFINED, 0, nullptr);
    JS_FreeValue(ctx, ret);
  }

  server->run(); // TODO: refactor this
  return JS_UNDEFINED;
}

static JSValue server_constructor(JSContext *ctx, JSValueConst new_target,
                                  int argc, JSValueConst *argv) {

  JSValue obj = JS_NewObjectClass(ctx, server_class_id);
  if (JS_IsException(obj))
    return obj;

  JSRuntime *rt = JS_GetRuntime(ctx);
  RuntimeContext *env = (RuntimeContext *)JS_GetRuntimeOpaque(rt);

  Server *server = env->server_pools->acquire();
  server->setEnv(env);

  JS_SetOpaque(obj, server);

  return obj;
}

void setup_server_class(JSContext *ctx) {
  JSRuntime *rt = JS_GetRuntime(ctx);
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