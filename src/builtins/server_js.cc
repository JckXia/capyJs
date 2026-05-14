#include "builtins/server_js.h"
#include "builtins.h"
#include "quickjs.h"
#include "server.h"
#include "types.h"
#include "util.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static void server_finalizer(JSRuntime *rt, JSValue val) {
  Server *server = (Server *)JS_GetOpaque(val, server_class_id);
  RuntimeContext *env = (RuntimeContext *)JS_GetRuntimeOpaque(rt);
  delete server;
}

MappedFile mmap_static_file(const char *path) {
  MappedFile result = {nullptr, 0, -1};

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    throw std::runtime_error(std::string("Static file not found: ") + path + " — " + strerror(errno));
  }
    //return result;

  struct stat st;
  if (fstat(fd, &st) < 0) {
    close(fd);
    return result;
  }

  void *mapped = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (mapped == MAP_FAILED) {
    close(fd);
    return result;
  }

  result.data = (char *)mapped;
  result.size = st.st_size;
  result.fd = fd;
  return result;
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

// Init on registration
static JSValue register_ws_url(JSContext *ctx, JSValueConst this_val, int argc,
                               JSValueConst *argv) {
  Server *server = (Server *)JS_GetOpaque2(ctx, this_val, server_class_id);
  if (!server)
    return JS_EXCEPTION;

  const char *uri = JS_ToCString(ctx, argv[0]);
  RuntimeContext *env =
      (RuntimeContext *)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
  JSValue callback = JS_DupValue(ctx, argv[1]);

  server->registerWsFuncHandler(uri, [ctx, callback](WebSocket &ws) {
    JSValue js_ws = JS_NewObjectClass(ctx, web_socket_class_id);
    JS_SetOpaque(js_ws, &ws);
    JSValue args[] = {js_ws};
    ws.sock_js = js_ws;
    JS_Call(ctx, callback, JS_UNDEFINED, 1, args);
 
  });
  JS_FreeCString(ctx, uri);
  env->http_ctx->registerd_cb.push_back(callback);
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

// server.serveStatic("uri", "./index.html"). This never goes back to JS land
// TODO: Infer header type from filepath extension (.js vs .css vs .html)
static JSValue server_serve_static(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
  Server *server = (Server *)JS_GetOpaque2(ctx, this_val, server_class_id);
  if (!server)
    return JS_EXCEPTION;

  const char *uri = JS_ToCString(ctx, argv[0]);
  const char *fp = JS_ToCString(ctx, argv[1]);
  RuntimeContext *env =
      (RuntimeContext *)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
  server->registerFuncHandler(
      "GET", uri, [env, fp](RequestObject &req, ResponseObject &res) {
        MappedFile f = env->http_ctx->static_files[fp];
        res.headers["Content-Type"] = get_content_type(get_file_extension(fp));
        res.header_size += (strlen("text/html") + strlen("Content-Type"));

        res.is_static = true;
        res.response_len = f.size;
        res.response_buffer = f.data;
        res.send();
      });

  return JS_UNDEFINED;
}

static JSValue server_constructor(JSContext *ctx, JSValueConst new_target,
                                  int argc, JSValueConst *argv) {

  JSValue server_obj = JS_NewObjectClass(ctx, server_class_id);
  if (JS_IsException(server_obj))
    return server_obj;

  JSRuntime *rt = JS_GetRuntime(ctx);
  RuntimeContext *env = (RuntimeContext *)JS_GetRuntimeOpaque(rt);

  Server * server = new Server();
  // TODO: Error handling, possibly throw an exception
  server->setEnv(env);

  JSValue staticFiles = JS_GetPropertyStr(ctx, argv[0], "staticFiles");
  int is_array = JS_IsArray(ctx, staticFiles);

  if (!is_array) {
    // TODO error handling
    return JS_UNDEFINED;
  }

  uint32_t length;
  JSValue len_val = JS_GetPropertyStr(ctx, staticFiles, "length");
  JS_ToUint32(ctx, &length, len_val);
  JS_FreeValue(ctx, len_val);

  for (uint32_t i = 0; i < length; i++) {
    JSValue elem = JS_GetPropertyUint32(ctx, staticFiles, i);

    const char *str;
    size_t len;

    str = JS_ToCStringLen(ctx, &len, elem);

    env->http_ctx->static_files[str] = mmap_static_file(str);
    JS_FreeValue(ctx, elem);
  }

  JS_FreeValue(ctx, staticFiles);
  JS_SetOpaque(server_obj, server);

  return server_obj;
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
  JS_SetPropertyStr(ctx, server_proto, "ws",
                    JS_NewCFunction(ctx, register_ws_url, "ws", 2));
  JS_SetPropertyStr(
      ctx, server_proto, "serveStatic",
      JS_NewCFunction(ctx, server_serve_static, "serveStatic", 2));
  JS_SetClassProto(ctx, server_class_id, server_proto);
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue ctor = JS_NewCFunction2(ctx, server_constructor, "Server", 0,
                                  JS_CFUNC_constructor, 0);

  JS_SetPropertyStr(ctx, global, "Server", ctor);
  JS_FreeValue(ctx, global);
}