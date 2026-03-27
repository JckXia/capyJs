#include "builtins/console.h"

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

void setup_console_class(JSContext* ctx) {
   JSValue global = JS_GetGlobalObject(ctx);
  JSValue console = JS_NewObject(ctx);
  JSValue func = JS_NewCFunction(ctx, console_log, "log", 1);
  JS_SetPropertyStr(ctx, console, "log", func);

  JS_SetPropertyStr(ctx, global, "console", console);
  JS_FreeValue(ctx, global);
}