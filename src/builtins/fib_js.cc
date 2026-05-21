#include "builtins/fib_js.h"
#include "fib_engine.h"
#include "runtime_context.h"
#include <cstring>

static JSClassID fib_class_id;
static JSClassDef fib_class_def = {.class_name = "NativeFibonacciManager"};

static void fib_finalizer(JSRuntime *rt, JSValue val) {
    NativeFibonacciManager *mgr =
        (NativeFibonacciManager *)JS_GetOpaque(val, fib_class_id);
    if (!mgr) return;
    // close() nulls the opaque, so reaching here means the user never called
    // close() explicitly. Stop workers and delete — safe, we're on the main
    // thread. JS callbacks in flight are freed without being called.
    mgr->js_close(nullptr);
    delete mgr;
}

// new NativeFibonacciManager({ worker_size: N })
static JSValue fib_ctor(JSContext *ctx, JSValueConst new_target,
                         int argc, JSValueConst *argv) {
    if (argc < 1 || !JS_IsObject(argv[0]))
        return JS_ThrowTypeError(ctx,
                                 "NativeFibonacciManager requires a config object");

    RuntimeContext *env =
        (RuntimeContext *)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));

    JSValue ws_val = JS_GetPropertyStr(ctx, argv[0], "worker_size");
    int worker_size = 1;
    JS_ToInt32(ctx, &worker_size, ws_val);
    JS_FreeValue(ctx, ws_val);
    if (worker_size < 1) worker_size = 1;

    auto *mgr = new NativeFibonacciManager(ctx, env->loop, worker_size);

    JSValue obj = JS_NewObjectClass(ctx, fib_class_id);
    JS_SetOpaque(obj, mgr);
    return obj;
}

// dispatch(value, callback) → job_id
static JSValue js_dispatch(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv) {
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "dispatch(value, callback)");

    NativeFibonacciManager *mgr =
        (NativeFibonacciManager *)JS_GetOpaque2(ctx, this_val, fib_class_id);
    if (!mgr) return JS_EXCEPTION;

    return mgr->js_dispatch(ctx, argv[0], argv[1]);
}

// getProgressByWorkerId(jobId) → "pending" | "done" | "error" | "unknown"
static JSValue js_get_progress(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "getProgressByWorkerId(workerId)");

    NativeFibonacciManager *mgr =
        (NativeFibonacciManager *)JS_GetOpaque2(ctx, this_val, fib_class_id);
    if (!mgr) return JS_EXCEPTION;

    return mgr->js_get_progress(ctx, argv[0]);
}

// close() → undefined  (async teardown of workers + handles)
static JSValue js_close(JSContext *ctx, JSValueConst this_val,
                         int argc, JSValueConst *argv) {
    NativeFibonacciManager *mgr =
        (NativeFibonacciManager *)JS_GetOpaque2(ctx, this_val, fib_class_id);
    if (!mgr) return JS_UNDEFINED;

    // Null the opaque so the GC finalizer is a no-op after explicit close.
    JS_SetOpaque(this_val, nullptr);
    return mgr->js_close(ctx);
}

// ---------------------------------------------------------------------------
// JS-level FibonacciManager wrapper — evaluated once at startup so user-facing
// code matches the API in the design doc exactly.
// ---------------------------------------------------------------------------
static const char k_fibonacci_manager_js[] =
    "class FibonacciManager {\n"
    "  constructor(opts) {\n"
    "    this._native = new NativeFibonacciManager(opts);\n"
    "  }\n"
    "  dispatch(opts, callback) {\n"
    "    return this._native.dispatch(opts.value, callback);\n"
    "  }\n"
    "  getProgressByWorkerId(workerId) {\n"
    "    return this._native.getProgressByWorkerId(workerId);\n"
    "  }\n"
    "  close() {\n"
    "    return this._native.close();\n"
    "  }\n"
    "}\n"
    "globalThis.FibonacciManager = FibonacciManager;\n";

void setup_fibonacci_class(JSContext *ctx) {
    JSRuntime *rt = JS_GetRuntime(ctx);
    JS_NewClassID(&fib_class_id);

    fib_class_def.finalizer = fib_finalizer;
    JS_NewClass(rt, fib_class_id, &fib_class_def);

    JSValue proto = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, proto, "dispatch",
        JS_NewCFunction(ctx, js_dispatch, "dispatch", 2));
    JS_SetPropertyStr(ctx, proto, "getProgressByWorkerId",
        JS_NewCFunction(ctx, js_get_progress, "getProgressByWorkerId", 1));
    JS_SetPropertyStr(ctx, proto, "close",
        JS_NewCFunction(ctx, js_close, "close", 0));
    JS_SetClassProto(ctx, fib_class_id, proto);

    JSValue ctor = JS_NewCFunction2(ctx, fib_ctor,
                                    "NativeFibonacciManager", 1,
                                    JS_CFUNC_constructor, 0);
    JS_SetConstructorBit(ctx, ctor, true);

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "NativeFibonacciManager", ctor);
    JS_FreeValue(ctx, global);

    JSValue result = JS_Eval(ctx, k_fibonacci_manager_js,
                             strlen(k_fibonacci_manager_js),
                             "<fibonacci_manager>", JS_EVAL_TYPE_GLOBAL);
    JS_FreeValue(ctx, result);
}
