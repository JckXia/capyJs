#pragma once
#include "quickjs.h"
#include "uv.h"
#include <string>

struct WSUpgradeInfo {
  bool isUpgrade;
  std::string key;
  std::string version;
  std::string protocol; // optional Sec-WebSocket-Protocol
};

struct WebSocket {
  size_t response_len = 0;
  char *response_buffer;

  const std::string ON_MESSAGE_JS_KEY = "onMessage";
  JSValue sock_js;
  uv_stream_t *cli;
  void onMessage(const std::string &s);
  int onMessage(const char *buffer, int buf_len);
  void send_frame();
  void complete_protocol_upgrade_handshake(const std::string &exchange_key);
};