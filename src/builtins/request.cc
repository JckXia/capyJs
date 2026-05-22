#include "builtins/request.h"
#include "http_message.h"
static JSClassDef request_class_def = {
    .class_name = "Request"
};   
JSClassID request_class_id;

static JSValue request_uri_getter(JSContext *ctx, JSValueConst this_val, int argc,
                            JSValueConst *argv) {
    RequestObject * req = (RequestObject*)JS_GetOpaque2(ctx, this_val, request_class_id);
    if(!req)
        return JS_EXCEPTION;
    
    return JS_NewString(ctx, req->uri);
}

static void add_uri_getter_attr(JSContext* ctx, JSValue& req_proto) {
    JSValue uri_getter_func = JS_NewCFunction(ctx, request_uri_getter, "uri", 0);
    JS_DefinePropertyGetSet(ctx, req_proto, 
                            JS_NewAtom(ctx, "uri"), 
                            uri_getter_func, 
                            JS_UNDEFINED, // Pass JS_UNDEFINED for no setter
                            JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
}

void setup_request_class(JSContext* ctx) {
    JSRuntime *rt = JS_GetRuntime(ctx);
    JS_NewClassID(&request_class_id);
    JS_NewClass(rt, request_class_id, &request_class_def);
    JSValue req_proto = JS_NewObject(ctx);

    add_uri_getter_attr(ctx, req_proto);
    JS_SetClassProto(ctx, request_class_id, req_proto);
}
