#pragma once
#include "quickjs.h"
void setup_set_timeout(JSContext * ctx);
void setup_set_immediate(JSContext *ctx);
void setup_set_interval(JSContext* ctx);
void setup_clear_interval(JSContext* ctx);