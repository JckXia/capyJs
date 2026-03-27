#include "builtins/request.h"
static JSClassDef request_class_def = {
    .class_name = "Request"
};   
JSClassID request_class_id;
void setup_request_class(JSContext* ctx) {
    JSRuntime *rt = JS_GetRuntime(ctx);
    JS_NewClassID(&request_class_id);
    JS_NewClass(rt, request_class_id, &request_class_def);
}
