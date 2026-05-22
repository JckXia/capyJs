#include "builtins/js_worker_js.h"
#include "js_engine.h"
#include "runtime_context.h"
#include <cstring>

static JSClassID js_worker_class_id;
static JSClassDef js_worker_class_def = {.class_name = "NativeJSWorkerManager"};

static void js_worker_finalizer(JSRuntime *rt, JSValue val) {
    NativeJSWorkerManager *mgr =
        (NativeJSWorkerManager *)JS_GetOpaque(val, js_worker_class_id);
    if (!mgr) return;
    mgr->js_close(nullptr);
    delete mgr;
}

// new NativeJSWorkerManager({ worker_size: N, worker_js: './worker.js' })
static JSValue js_worker_ctor(JSContext *ctx, JSValueConst new_target,
                               int argc, JSValueConst *argv) {
    if (argc < 1 || !JS_IsObject(argv[0]))
        return JS_ThrowTypeError(ctx,
                                 "NativeJSWorkerManager requires a config object");

    RuntimeContext *env =
        (RuntimeContext *)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));

    JSValue ws_val = JS_GetPropertyStr(ctx, argv[0], "worker_size");
    int worker_size = 1;
    JS_ToInt32(ctx, &worker_size, ws_val);
    JS_FreeValue(ctx, ws_val);
    if (worker_size < 1) worker_size = 1;

    JSValue wjs_val = JS_GetPropertyStr(ctx, argv[0], "worker_js");
    const char *worker_js_cstr = JS_ToCString(ctx, wjs_val);
    JS_FreeValue(ctx, wjs_val);
    if (!worker_js_cstr)
        return JS_ThrowTypeError(ctx, "worker_js must be a string path");

    std::string worker_js_path(worker_js_cstr);
    JS_FreeCString(ctx, worker_js_cstr);

    auto *mgr = new NativeJSWorkerManager(ctx, env->loop, worker_size,
                                           worker_js_path);

    JSValue obj = JS_NewObjectClass(ctx, js_worker_class_id);
    JS_SetOpaque(obj, mgr);
    return obj;
}

static JSValue jsw_dispatch(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv) {
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "dispatch(inputJson, callback)");

    NativeJSWorkerManager *mgr =
        (NativeJSWorkerManager *)JS_GetOpaque2(ctx, this_val, js_worker_class_id);
    if (!mgr) return JS_EXCEPTION;

    return mgr->js_dispatch(ctx, argv[0], argv[1]);
}

static JSValue jsw_get_progress(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "getProgress(jobId)");

    NativeJSWorkerManager *mgr =
        (NativeJSWorkerManager *)JS_GetOpaque2(ctx, this_val, js_worker_class_id);
    if (!mgr) return JS_EXCEPTION;

    return mgr->js_get_progress(ctx, argv[0]);
}

static JSValue jsw_close(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv) {
    NativeJSWorkerManager *mgr =
        (NativeJSWorkerManager *)JS_GetOpaque2(ctx, this_val, js_worker_class_id);
    if (!mgr) return JS_UNDEFINED;

    JS_SetOpaque(this_val, nullptr);
    return mgr->js_close(ctx);
}

static const char k_js_worker_manager_js[] =
    "class JSWorkerManager {\n"
    "  constructor(opts) {\n"
    "    this._native = new NativeJSWorkerManager(opts);\n"
    "  }\n"
    "  dispatch(inputJson, callback) {\n"
    "    return this._native.dispatch(inputJson, callback);\n"
    "  }\n"
    "  getProgress(jobId) {\n"
    "    return this._native.getProgress(jobId);\n"
    "  }\n"
    "  close() {\n"
    "    return this._native.close();\n"
    "  }\n"
    "}\n"
    "globalThis.JSWorkerManager = JSWorkerManager;\n";

void setup_js_worker_class(JSContext *ctx) {
    JSRuntime *rt = JS_GetRuntime(ctx);
    JS_NewClassID(&js_worker_class_id);

    js_worker_class_def.finalizer = js_worker_finalizer;
    JS_NewClass(rt, js_worker_class_id, &js_worker_class_def);

    JSValue proto = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, proto, "dispatch",
        JS_NewCFunction(ctx, jsw_dispatch, "dispatch", 2));
    JS_SetPropertyStr(ctx, proto, "getProgress",
        JS_NewCFunction(ctx, jsw_get_progress, "getProgress", 1));
    JS_SetPropertyStr(ctx, proto, "close",
        JS_NewCFunction(ctx, jsw_close, "close", 0));
    JS_SetClassProto(ctx, js_worker_class_id, proto);

    JSValue ctor = JS_NewCFunction2(ctx, js_worker_ctor,
                                    "NativeJSWorkerManager", 1,
                                    JS_CFUNC_constructor, 0);
    JS_SetConstructorBit(ctx, ctor, true);

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "NativeJSWorkerManager", ctor);
    JS_FreeValue(ctx, global);

    JSValue result = JS_Eval(ctx, k_js_worker_manager_js,
                             strlen(k_js_worker_manager_js),
                             "<js_worker_manager>", JS_EVAL_TYPE_GLOBAL);
    JS_FreeValue(ctx, result);
}
