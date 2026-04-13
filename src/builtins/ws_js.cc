#include "builtins/ws_js.h"
#include "builtins/response.h"
#include "http_message.h"
#include "runtime_context.h" // Do we really need this to get allocator?
#include <string.h>

static JSClassDef web_socket_class_id_def = {.class_name = "WebSocket"};
JSClassID web_socket_class_id;

static JSValue send(JSContext *ctx, JSValueConst this_val, int argc,
                    JSValueConst *argv) {
  std::cout << "Called send! \n";
  WebSocket *ws =
      (WebSocket *)JS_GetOpaque2(ctx, this_val, web_socket_class_id);

  if (!ws)
    return JS_EXCEPTION;
  JSRuntime *rt = JS_GetRuntime(ctx);
  RuntimeContext *env =
      (RuntimeContext *)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
  const char *body = JS_ToCString(ctx, argv[0]);
  size_t len = strlen(body);
    
  return JS_UNDEFINED;
}

void setup_web_socket_class(JSContext *ctx) {
  JSRuntime *rt = JS_GetRuntime(ctx);
  JS_NewClassID(&web_socket_class_id);
  JS_NewClass(rt, web_socket_class_id, &web_socket_class_id_def);
  JSValue ws_proto = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, ws_proto, "send",
                    JS_NewCFunction(ctx, send, "send", 1));

  JS_SetClassProto(ctx, web_socket_class_id, ws_proto);
}