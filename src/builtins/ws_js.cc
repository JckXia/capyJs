#include "builtins/ws_js.h"
#include "builtins/response.h"
#include "http_message.h"
#include <string.h>
#include "runtime_context.h" // Do we really need this to get allocator?

static JSClassDef web_socket_class_id_def = {.class_name = "WebSocket"};
JSClassID web_socket_class_id;
 

static JSValue on_message_handler(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst * argv) {
    return JS_UNDEFINED;
}

static JSValue on_close_handler(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst * argv) {
    return JS_UNDEFINED;
}

static JSValue send(JSContext * ctx, JSValueConst this_val, int argc, JSValueConst * argv) {
    return JS_UNDEFINED;
}

void setup_web_socket_class(JSContext *ctx) {
  JSRuntime *rt = JS_GetRuntime(ctx);
  JS_NewClassID(&web_socket_class_id);
  JS_NewClass(rt, web_socket_class_id, &web_socket_class_id_def);
  JSValue ws_proto = JS_NewObject(ctx);
//   JS_SetPropertyStr(ctx, ws_proto, "onmessage", JS_NewCFunction(ctx, on_message_handler, "onmessage", 1));
//   JS_SetPropertyStr(ctx, ws_proto, "onclose", JS_NewCFunction(ctx, on_close_handler, "onclose", 1));
  JS_SetPropertyStr(ctx, ws_proto, "send", JS_NewCFunction(ctx, send,"send",1));

  JS_SetClassProto(ctx, web_socket_class_id, ws_proto);
}