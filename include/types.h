# pragma once
enum AllocType : uint8_t {
  TYPE_UNKNOWN = 0,
  TYPE_CLIENT_STATE = 1,
  TYPE_READ_BUFFER = 2,
  TYPE_WRITE_BUFFER = 3,
  TYPE_RESPONSE = 4,
  TYPE_UV_TCP = 5,
  TYPE_UV_WRITE = 6,
  TYPE_WS = 7,
};

// human readable names for visualizer:
static const char *TYPE_NAMES[] = {
    "Unknown",        "ClientState", "ReadBuffer", "WriteBuffer",
    "ResponseObject", "uv_tcp_t",    "uv_write_t", "WebSocket"
};