#include "builtins/inference_js.h"
#include "inference_engine.h"
#include "runtime_context.h"
#include <cstring>
#include <unistd.h>

static JSClassID inf_class_id;
static JSClassDef inf_class_def = {.class_name = "NativeInferenceEngine"};

static void inf_finalizer(JSRuntime *rt, JSValue val) {
    NativeInferenceManager *mgr =
        (NativeInferenceManager *)JS_GetOpaque(val, inf_class_id);
    if (!mgr) return;
    mgr->js_close(nullptr);
    delete mgr;
}

// new NativeInferenceEngine({ models: [{model_class, path}], workers: N })
static JSValue inf_ctor(JSContext *ctx, JSValueConst new_target,
                         int argc, JSValueConst *argv) {
    if (argc < 1 || !JS_IsObject(argv[0]))
        return JS_ThrowTypeError(ctx, "NativeInferenceEngine requires a config object");

    RuntimeContext *env =
        (RuntimeContext *)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));

    auto *mgr = new NativeInferenceManager(ctx, env->loop);

    bool use_mlock = false;
    JSValue mlv = JS_GetPropertyStr(ctx, argv[0], "mlock");
    if (JS_IsBool(mlv)) use_mlock = (bool)JS_ToBool(ctx, mlv);
    JS_FreeValue(ctx, mlv);

    int n_ctx = 2048;
    JSValue nctxv = JS_GetPropertyStr(ctx, argv[0], "n_ctx");
    if (JS_IsNumber(nctxv)) { int v; if (JS_ToInt32(ctx, &v, nctxv) == 0 && v > 0) n_ctx = v; }
    JS_FreeValue(ctx, nctxv);

    int n_batch = 0; // 0 = default to n_ctx inside spawn_workers
    JSValue nbv = JS_GetPropertyStr(ctx, argv[0], "n_batch");
    if (JS_IsNumber(nbv)) { int v; if (JS_ToInt32(ctx, &v, nbv) == 0 && v > 0) n_batch = v; }
    JS_FreeValue(ctx, nbv);

    JSValue models = JS_GetPropertyStr(ctx, argv[0], "models");
    if (JS_IsArray(ctx, models)) {
        JSValue len_val = JS_GetPropertyStr(ctx, models, "length");
        int len = 0;
        JS_ToInt32(ctx, &len, len_val);
        JS_FreeValue(ctx, len_val);

        for (int i = 0; i < len; i++) {
            JSValue entry    = JS_GetPropertyUint32(ctx, models, i);
            JSValue cls_val  = JS_GetPropertyStr(ctx, entry, "model_class");
            JSValue path_val = JS_GetPropertyStr(ctx, entry, "path");

            const char *cls  = JS_ToCString(ctx, cls_val);
            const char *path = JS_ToCString(ctx, path_val);
            if (cls && path) mgr->add_model(cls, path, use_mlock);

            JS_FreeCString(ctx, cls);
            JS_FreeCString(ctx, path);
            JS_FreeValue(ctx, cls_val);
            JS_FreeValue(ctx, path_val);
            JS_FreeValue(ctx, entry);
        }
    }
    JS_FreeValue(ctx, models);

    int worker_count = 1;
    JSValue wv = JS_GetPropertyStr(ctx, argv[0], "workers");
    if (JS_IsNumber(wv)) {
        int n;
        if (JS_ToInt32(ctx, &n, wv) == 0 && n > 0)
            worker_count = n;
    }
    JS_FreeValue(ctx, wv);

    mgr->spawn_workers(worker_count, n_ctx, n_batch);

    JSValue obj = JS_NewObjectClass(ctx, inf_class_id);
    JS_SetOpaque(obj, mgr);
    return obj;
}

static JSValue js_do_inference(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "do_inference(packet, callback)");

    NativeInferenceManager *mgr =
        (NativeInferenceManager *)JS_GetOpaque2(ctx, this_val, inf_class_id);
    if (!mgr) return JS_EXCEPTION;

    return mgr->js_dispatch(ctx, argv[0], argv[1]);
}

static JSValue js_list_workers(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    NativeInferenceManager *mgr =
        (NativeInferenceManager *)JS_GetOpaque2(ctx, this_val, inf_class_id);
    if (!mgr) return JS_EXCEPTION;
    return mgr->js_list_workers(ctx);
}

static JSValue js_get_progress(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "get_progress(job_id)");
    NativeInferenceManager *mgr =
        (NativeInferenceManager *)JS_GetOpaque2(ctx, this_val, inf_class_id);
    if (!mgr) return JS_EXCEPTION;
    return mgr->js_get_progress(ctx, argv[0]);
}

static JSValue js_close(JSContext *ctx, JSValueConst this_val,
                         int argc, JSValueConst *argv) {
    NativeInferenceManager *mgr =
        (NativeInferenceManager *)JS_GetOpaque2(ctx, this_val, inf_class_id);
    if (!mgr) return JS_UNDEFINED;
    JS_SetOpaque(this_val, nullptr); // prevent double-free by GC finalizer
    JSValue ret = mgr->js_close(ctx);
    delete mgr;
    return ret;
}

// JS-level InferenceEngine wrapper exposing the API from the design doc.
static const char k_inference_engine_js[] =
    "class InferenceEngine {\n"
    "  constructor(opts) {\n"
    "    this._native = new NativeInferenceEngine(opts);\n"
    "  }\n"
    "  do_inference(packet, callback) {\n"
    "    return this._native.do_inference(packet, callback);\n"
    "  }\n"
    "  list_workers() {\n"
    "    return this._native.list_workers();\n"
    "  }\n"
    "  get_progress(job_id) {\n"
    "    return this._native.get_progress(job_id);\n"
    "  }\n"
    "  close() {\n"
    "    return this._native.close();\n"
    "  }\n"
    "}\n"
    "globalThis.InferenceEngine = InferenceEngine;\n";

void setup_inference_engine_class(JSContext *ctx) {
    JSRuntime *rt = JS_GetRuntime(ctx);
    JS_NewClassID(&inf_class_id);

    inf_class_def.finalizer = inf_finalizer;
    JS_NewClass(rt, inf_class_id, &inf_class_def);

    JSValue proto = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, proto, "do_inference",
        JS_NewCFunction(ctx, js_do_inference, "do_inference", 2));
    JS_SetPropertyStr(ctx, proto, "list_workers",
        JS_NewCFunction(ctx, js_list_workers, "list_workers", 0));
    JS_SetPropertyStr(ctx, proto, "get_progress",
        JS_NewCFunction(ctx, js_get_progress, "get_progress", 1));
    JS_SetPropertyStr(ctx, proto, "close",
        JS_NewCFunction(ctx, js_close, "close", 0));
    JS_SetClassProto(ctx, inf_class_id, proto);

    JSValue ctor = JS_NewCFunction2(ctx, inf_ctor,
                                    "NativeInferenceEngine", 1,
                                    JS_CFUNC_constructor, 0);
    JS_SetConstructorBit(ctx, ctor, true);

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "NativeInferenceEngine", ctor);
    JS_FreeValue(ctx, global);

    JSValue result = JS_Eval(ctx, k_inference_engine_js,
                             strlen(k_inference_engine_js),
                             "<inference_engine>", JS_EVAL_TYPE_GLOBAL);
    JS_FreeValue(ctx, result);
}
