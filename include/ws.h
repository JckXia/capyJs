#pragma once
#include <string>
#include "uv.h"
#include "quickjs.h"
struct WSUpgradeInfo {
    bool isUpgrade;
    std::string key;
    std::string version;
    std::string protocol;  // optional Sec-WebSocket-Protocol
};

struct WebSocket {
    size_t response_len = 0;
    char *response_buffer;

    const std::string ON_MESSAGE_JS_KEY = "onMessage";
    JSValue sock_js;     
    uv_stream_t *cli;
    void onMessage(); 
    void send_frame();
    void complete_protocol_upgrade_handshake(const std::string& exchange_key);
}; 