#pragma once
#include "builtins/console.h"
#include "builtins/request.h"
#include "builtins/response.h"
#include "builtins/server_js.h"
#include "quickjs.h"
// Single entry point
void setup_all_builtins(JSContext *ctx);

// extern JSClassID consoles_class_id;
// extern JSClassID request_class_id;
// extern JSClassID server_class_id;
// extern JSClassID request_class_id;
// extern JSClassID response_class_id;

// Or individual if needed
// void setup_console(JSContext* ctx);
// void setup_server_class(JSContext* ctx);
// void setup_request_class(JSContext* ctx);
// void setup_response_class(JSContext* ctx);