#include <iostream>
#include "uv.h"
#include "server.h"

int main() {
    Server s(9091,"0.0.0.0");
    s.registerFuncHandler("GET", "/hello", [](RequestObject& req, ResponseObject& res) {
        const char *msg = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/plain\r\n"
                      "Content-Length: 25\r\n"
                      "Connection: keep-alive\r\n"
                      "\r\n"
                      "hallo from capyJS Server\n";

        res.response = msg;
        res.response_len = strlen(msg); // Can be abstracted away really. 
    });
    s.run();
}