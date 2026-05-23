#pragma once
#include "http_message.h"
#include "conn_guard.h"
#include "uv.h"
#include <cstring>
#include "ws.h"

class Server;
struct ReadBuffer {
  ReadBuffer() : next(nullptr), len(0) {}
  char read_buffer[256];
  size_t len;
  ReadBuffer *next;
};

enum ConnectionProtocol : uint8_t {
  TYPE_UNKNOWN_CONN = 0,
  TYPE_HTTP_1 = 1,
  TYPE_WEB_SOCKET = 2
};

static const char *CONN_PROTOCOL[] = {"Unknown", "HTTP_1.1", "WebSocket"};

// TODO: We are stuffing QuickJS into ClientState. Not sure if this is a "good" idea
//  -> For perf sake we'll just hack it rn?
struct ClientState {
  uv_tcp_t socket;
  uv_timer_t idle_timer;
  uv_write_t write_handle;
  bool write_in_flight = false;
  bool closing = false;
  uint8_t conn_protocol =
      ConnectionProtocol::TYPE_HTTP_1; // Default to HTTP 1.1
  ReadBuffer *recv_head;
  ReadBuffer *recv_tail;

  char *pending_write_buffer = nullptr; // TODO: Need to rethink this bit
  WebSocket *activeWs = nullptr;
  Server *server = nullptr;
  ConnGuard *guard = nullptr;
  size_t recv_count = 0;
  int recv_len = 0;

  ClientState() : recv_head(nullptr), recv_tail(nullptr) {}
};
