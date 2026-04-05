#include "builtins/response.h"
#include "http_message.h"
#include <string.h>
static JSClassDef response_class_def = {.class_name = "Response"};
JSClassID response_class_id;
static JSValue response_end(JSContext *ctx, JSValueConst this_val, int argc,
                            JSValueConst *argv) {
  ResponseObject *res =
      (ResponseObject *)JS_GetOpaque2(ctx, this_val, response_class_id);
  if (!res)
    return JS_EXCEPTION;

  const char *body = JS_ToCString(ctx, argv[0]);
  size_t len = strlen(body);

  if (len >= sizeof(res->response_buf)) {
    JS_FreeCString(ctx, body);
    return JS_ThrowRangeError(ctx, "Response too large");
  }

  memcpy(res->response_buf, body, len + 1); // Copy into your buffer
  res->response_len = len;

  JS_FreeCString(ctx, body); // Safe to free now

  res->send();
  return JS_UNDEFINED;
}

static JSValue response_send(JSContext *ctx, JSValueConst this_val, int argc,
                             JSValueConst *argv) {
  ResponseObject *res =
      (ResponseObject *)JS_GetOpaque2(ctx, this_val, response_class_id);
  if (!res)
    return JS_EXCEPTION;

  const char *body = JS_ToCString(ctx, argv[0]);
  size_t len = strlen(body);

  if (len >= sizeof(res->response_buf)) {
    JS_FreeCString(ctx, body);
    return JS_ThrowRangeError(ctx, "Response too large");
  }

  memcpy(res->response_buf, body, len + 1); // Copy into your buffer
  res->response_len = len;

  JS_FreeCString(ctx, body); // Safe to free now
  return JS_UNDEFINED;
}

// TODO: Make the response header object setter.
static JSValue response_set_header(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
  ResponseObject *res =
      (ResponseObject *)JS_GetOpaque2(ctx, this_val, response_class_id);
  if (!res)
    return JS_EXCEPTION;

  const char *header_key = strdup(JS_ToCString(ctx, argv[0]));
  const char *header_val = strdup(JS_ToCString(ctx, argv[1]));
  res->headers[header_key] = header_val;
  res->header_size += strlen(header_val);
  return JS_UNDEFINED;
}

void setup_response_class(JSContext *ctx) {
  JSRuntime *rt = JS_GetRuntime(ctx);
  JS_NewClassID(&response_class_id);
  JS_NewClass(rt, response_class_id, &response_class_def);
  JSValue res_proto = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, res_proto, "send",
                    JS_NewCFunction(ctx, response_send, "send", 1));
  JS_SetPropertyStr(ctx, res_proto, "end",
                    JS_NewCFunction(ctx, response_end, "end", 1));
  JS_SetPropertyStr(ctx, res_proto, "setHeader",
                    JS_NewCFunction(ctx, response_set_header, "setHeader", 2));
  JS_SetClassProto(ctx, response_class_id, res_proto);
}