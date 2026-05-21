#pragma once
#include "client_state.h"
#include "uv.h"
#include <cstddef>
#include <map>

struct RequestObject {
  char verb[16];
  char uri[512];
};

struct ResponseObject {
  size_t response_len = 0;
  char *response_buffer;
  RequestObject *req;
  std::map<const char *, const char *> headers;
  int header_size = 0;

  bool is_static = false;
  uv_stream_t *cli;

  static int build_headers(char *header_buf, size_t header_len,
                           ResponseObject *res);

  static void on_shutdown_cb(uv_shutdown_t*req, int status);
  void send();
  void abort();
};
