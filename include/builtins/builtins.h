#pragma once
#include "builtins/console.h"
#include "builtins/request.h"
#include "builtins/response.h"
#include "builtins/server_js.h"
#include "builtins/timer.h"
#include "builtins/fs.h"
#include "builtins/ws_js.h"
#include "builtins/fib_js.h"
#include "quickjs.h"
// Single entry point
void setup_all_builtins(JSContext *ctx);