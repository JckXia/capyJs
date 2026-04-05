#pragma once
#include "client_state.h"
#include "uv.h"
#include <cstddef>
#include <iostream>
#include <map> // TODO: All of this needs to be repalced with deterministinc alloc

struct RequestObject {
  char verb[7];
  char uri[20];
};

// TODO: Refactor this
struct ResponseObject {
  size_t response_len = 0;
  char *response_buffer;

  std::map<const char *, const char *> headers;
  int header_size = 0;

  bool is_static = false;
  uv_stream_t *cli;

  static int build_headers(char *header_buf, size_t header_len,
                           ResponseObject *res);

  static void on_write_cb(uv_write_t *req, int status);
  static void on_static_write_cb(uv_write_t *req, int status);
  void send();
};
