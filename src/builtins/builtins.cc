#include "builtins/builtins.h"

void setup_all_builtins(JSContext *ctx) {
  setup_console_class(ctx);
  setup_request_class(ctx);
  setup_response_class(ctx);
  setup_server_class(ctx);
}