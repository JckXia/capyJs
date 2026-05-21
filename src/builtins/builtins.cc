#include "builtins/builtins.h"

void setup_all_builtins(JSContext *ctx) {
  setup_console_class(ctx);
  setup_request_class(ctx);
  setup_response_class(ctx);
  setup_server_class(ctx);
  setup_set_timeout(ctx);
  setup_set_immediate(ctx);
  setup_set_interval(ctx);
  setup_clear_interval(ctx);
  setup_fs_class(ctx);
  setup_web_socket_class(ctx);
  setup_fibonacci_class(ctx);
}