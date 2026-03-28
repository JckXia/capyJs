#include "builtins/timer.h"
#include "uv.h"
#include "runtime_context.h"
#include <ctime>


struct Libuv_Cb {
    JSValue js_cb;
    JSContext * ctx;
};

void on_close(uv_handle_t * handle) {
    delete (Libuv_Cb *) handle->data;
    free(handle);
}

void timer_libuv_cb(uv_timer_t * handle) {
    Libuv_Cb* cb = (Libuv_Cb*)handle->data;
    JSContext * js_ctx = cb->ctx;
    JSValue js_cb =cb->js_cb;
    
    JS_Call(js_ctx,js_cb, JS_UNDEFINED, 0, nullptr);
    uv_close((uv_handle_t*) handle, on_close);
}

void set_interval_libuv_cb(uv_timer_t * handle) {
    Libuv_Cb* cb = (Libuv_Cb*)handle->data;
    JSContext * js_ctx = cb->ctx;
    JSValue js_cb =cb->js_cb;
    
    JS_Call(js_ctx,js_cb, JS_UNDEFINED, 0, nullptr);
}
int randomInt(int min, int max) {
    return min + rand() % (max - min + 1);
}

static JSValue SetTimeOutFunc(JSContext * ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int32_t delay;
    JS_ToInt32(ctx, &delay, argv[1]);
    JSValue js_cb = JS_DupValue(ctx, argv[0]);
    RuntimeContext *env =
      (RuntimeContext *)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    env->timer_ctx->registered_cb.push_back(js_cb);

    Libuv_Cb *cb = new Libuv_Cb();
    cb->js_cb = js_cb;
    cb->ctx = ctx;

    uv_timer_t* handle = (uv_timer_t*) malloc(sizeof(uv_timer_t));
    uv_timer_init(env->loop, handle);
    handle->data = cb;
    uv_timer_start(handle, timer_libuv_cb, delay, 0);

    return JS_UNDEFINED;
}

static JSValue SetIntervalFunc(JSContext * ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int32_t delay;
    JS_ToInt32(ctx, &delay, argv[1]);
    JSValue js_cb = JS_DupValue(ctx, argv[0]);
    RuntimeContext *env =
      (RuntimeContext *)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    env->timer_ctx->registered_cb.push_back(js_cb);

    Libuv_Cb *cb = new Libuv_Cb();
    cb->js_cb = js_cb;
    cb->ctx = ctx;

    uv_timer_t* handle = (uv_timer_t*) malloc(sizeof(uv_timer_t));
    uv_timer_init(env->loop, handle);
    handle->data = cb;
    uv_timer_start(handle, set_interval_libuv_cb, delay, delay);


    int timerId = randomInt(1,1000);
 
    env->timer_ctx->registered_interval_cb[timerId] = handle;

    return JS_NewInt32(ctx, timerId);
}

static JSValue clearIntervalFunc(JSContext * ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int32_t timerId;
    JS_ToInt32(ctx, &timerId, argv[0]);
    RuntimeContext *env =
      (RuntimeContext *)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    uv_timer_t * handle = env->timer_ctx->registered_interval_cb[timerId];
    uv_close((uv_handle_t*) handle, on_close);
    return JS_UNDEFINED;
}

void setup_set_timeout(JSContext * ctx) {
 
 JSValue global= JS_GetGlobalObject(ctx);
 JSValue func = JS_NewCFunction(ctx, SetTimeOutFunc,"setTimeout" ,2);
 JS_SetPropertyStr(ctx, global, "setTimeout", func);
 JS_FreeValue(ctx, global);
}

void setup_set_immediate(JSContext *ctx) {

}

void setup_set_interval(JSContext* ctx) {
 srand(time(nullptr)); // TODO: Move this into the util function 
 JSValue global= JS_GetGlobalObject(ctx);
 JSValue func = JS_NewCFunction(ctx, SetIntervalFunc,"setInterval" ,2);
 JS_SetPropertyStr(ctx, global, "setInterval", func);
 JS_FreeValue(ctx, global);
}

void setup_clear_interval(JSContext* ctx) {
 JSValue global= JS_GetGlobalObject(ctx);
 JSValue func = JS_NewCFunction(ctx, clearIntervalFunc,"clearInterval" ,1);
 JS_SetPropertyStr(ctx, global, "clearInterval", func);
 JS_FreeValue(ctx, global);
}