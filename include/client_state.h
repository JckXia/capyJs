#pragma once
#include "http_message.h"
#include "uv.h"
#include <cstring>
struct ReadBuffer {
  ReadBuffer() : next(nullptr), len(0) {}
  char read_buffer[256];
  size_t len;
  ReadBuffer *next;
};

struct ClientState {
  uv_tcp_t socket;
  uv_write_t write_handle;
  bool write_in_flight = false;
  ReadBuffer *recv_head;
  ReadBuffer *recv_tail;

  char *pending_write_buffer = nullptr; // TODO: Need to rethink this bit
  
  size_t recv_count = 0;
  int recv_len = 0;

  ClientState() : recv_head(nullptr), recv_tail(nullptr) {}
};
