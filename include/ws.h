#pragma once
#include <string>
struct WSUpgradeInfo {
    bool isUpgrade;
    std::string key;
    std::string version;
    std::string protocol;  // optional Sec-WebSocket-Protocol
};

WSUpgradeInfo parseWSUpgrade(phr_header* headers, size_t num_headers) {
    WSUpgradeInfo info = {false, "", "", ""};
    
    bool hasUpgrade = false;
    bool hasConnection = false;
    
    for (size_t i = 0; i < num_headers; i++) {
        std::string name(headers[i].name, headers[i].name_len);
        std::string value(headers[i].value, headers[i].value_len);
        
        // Case-insensitive compare
        if (name.size() == 7 && strncasecmp(name.c_str(), "Upgrade", 7) == 0) {
            hasUpgrade = (strncasecmp(value.c_str(), "websocket", 9) == 0);
        }
        else if (name.size() == 10 && strncasecmp(name.c_str(), "Connection", 10) == 0) {
            // Connection might be "Upgrade" or "keep-alive, Upgrade"
            hasConnection = (strcasestr(value.c_str(), "upgrade") != nullptr);
        }
        else if (name.size() == 17 && strncasecmp(name.c_str(), "Sec-WebSocket-Key", 17) == 0) {
            info.key = value;
        }
        else if (name.size() == 21 && strncasecmp(name.c_str(), "Sec-WebSocket-Version", 21) == 0) {
            info.version = value;
        }
        else if (name.size() == 22 && strncasecmp(name.c_str(), "Sec-WebSocket-Protocol", 22) == 0) {
            info.protocol = value;
        }
    }
    
    info.isUpgrade = hasUpgrade && hasConnection && !info.key.empty();
    return info;
}