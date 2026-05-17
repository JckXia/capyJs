#include "builtins/inference_js.h"
#include "inference_engine.h"
#include "runtime_context.h"
#include <cstring>
#include <unistd.h>

static JSClassID inference_engine_class_id;
static JSClassDef inference_engine_class_def = {.class_name = "InferenceEngine"};

static void inference_engine_finalizer(JSRuntime *rt, JSValue val) {
    InferenceEngine *engine =
        (InferenceEngine *)JS_GetOpaque(val, inference_engine_class_id);
    if (!engine) return;

    engine->shutdown();

    uv_async_t *ah = engine->async_handle();
    ah->data = engine;
    uv_close((uv_handle_t *)ah, [](uv_handle_t *h) {
        delete (InferenceEngine *)h->data;
    });
}

// new InferenceEngine({ models: [{ model_class, path }], workers: N })
// workers defaults to nprocs - 1 (core 0 reserved for event loop)
static JSValue inference_engine_ctor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv) {
    if (argc < 1 || !JS_IsObject(argv[0]))
        return JS_ThrowTypeError(ctx, "InferenceEngine requires a config object");

    RuntimeContext *env =
        (RuntimeContext *)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));

    InferenceEngine *engine = new InferenceEngine(ctx, env->loop);

    JSValue models = JS_GetPropertyStr(ctx, argv[0], "models");
    if (JS_IsArray(ctx, models)) {
        JSValue len_val = JS_GetPropertyStr(ctx, models, "length");
        int len;
        JS_ToInt32(ctx, &len, len_val);
        JS_FreeValue(ctx, len_val);

        for (int i = 0; i < len; i++) {
            JSValue entry    = JS_GetPropertyUint32(ctx, models, i);
            JSValue cls_val  = JS_GetPropertyStr(ctx, entry, "model_class");
            JSValue path_val = JS_GetPropertyStr(ctx, entry, "path");

            const char *cls  = JS_ToCString(ctx, cls_val);
            const char *path = JS_ToCString(ctx, path_val);

            if (cls && path) engine->add_model(cls, path);

            JS_FreeCString(ctx, cls);
            JS_FreeCString(ctx, path);
            JS_FreeValue(ctx, cls_val);
            JS_FreeValue(ctx, path_val);
            JS_FreeValue(ctx, entry);
        }
    }
    JS_FreeValue(ctx, models);

    // Determine worker count: explicit config > auto-detect (nprocs - 1, min 1)
    int worker_count = (int)sysconf(_SC_NPROCESSORS_ONLN) - 1;
    if (worker_count < 1) worker_count = 1;

    JSValue workers_val = JS_GetPropertyStr(ctx, argv[0], "workers");
    if (!JS_IsUndefined(workers_val) && !JS_IsNull(workers_val)) {
        int n;
        if (JS_ToInt32(ctx, &n, workers_val) == 0 && n > 0)
            worker_count = n;
    }
    JS_FreeValue(ctx, workers_val);

    engine->spawn_workers(worker_count);

    JSValue obj = JS_NewObjectClass(ctx, inference_engine_class_id);
    JS_SetOpaque(obj, engine);
    return obj;
}

static JSValue js_list_workers(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    InferenceEngine *engine =
        (InferenceEngine *)JS_GetOpaque2(ctx, this_val, inference_engine_class_id);
    if (!engine) return JS_EXCEPTION;
    return engine->js_list_workers(ctx);
}

// do_inference(model_class, { session_id, prompt }, (err, token) => {})
static JSValue js_do_inference(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    if (argc < 3)
        return JS_ThrowTypeError(ctx, "do_inference(model_class, packet, callback)");

    InferenceEngine *engine =
        (InferenceEngine *)JS_GetOpaque2(ctx, this_val, inference_engine_class_id);
    if (!engine) return JS_EXCEPTION;

    return engine->js_do_inference(ctx, argv[0], argv[1], argv[2]);
}

void setup_inference_engine_class(JSContext *ctx) {
    JSRuntime *rt = JS_GetRuntime(ctx);
    JS_NewClassID(&inference_engine_class_id);

    inference_engine_class_def.finalizer = inference_engine_finalizer;
    JS_NewClass(rt, inference_engine_class_id, &inference_engine_class_def);

    JSValue proto = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, proto, "list_workers",
                      JS_NewCFunction(ctx, js_list_workers, "list_workers", 0));
    JS_SetPropertyStr(ctx, proto, "do_inference",
                      JS_NewCFunction(ctx, js_do_inference, "do_inference", 3));
    JS_SetClassProto(ctx, inference_engine_class_id, proto);

    JSValue ctor = JS_NewCFunction2(ctx, inference_engine_ctor,
                                    "InferenceEngine", 1, JS_CFUNC_constructor, 0);
    JS_SetConstructorBit(ctx, ctor, true);

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "InferenceEngine", ctor);
    JS_FreeValue(ctx, global);
}
