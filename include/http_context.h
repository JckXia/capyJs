#pragma once
#include "client_state.h"
#include <functional>
#include "quickjs.h"
#include "mapped_file.h"
#include <vector>
#include <map>
#include "ws.h"

using Handler = std::function<void(RequestObject &, ResponseObject &)>;
using WSHandler = std::function<void(WebSocket &)>;

struct HttpContext {
  std::vector<JSValue> registerd_cb;
  std::map<std::string, MappedFile> static_files;
  std::map<std::pair<std::string, std::string>, Handler> routes_;
  std::map<std::string, WSHandler> ws_routes_;

  void register_api_function(const char *method, const char *uri,
                             Handler handler) {
    routes_[{method, uri}] = handler;
  }

  void register_ws_cb(const char *uri, WSHandler handler) {
    ws_routes_[uri] = handler;
  }

  void invoke_function(RequestObject &req, ResponseObject &res,
                       const char *method, const char *uri) {
    auto it = routes_.find({req.verb, req.uri});
    if (it != routes_.end()) {
      it->second(req, res);
    } else {
      // std::cout << "[404] " << req.verb << " " << req.uri << "\n";
      static const char response[] =
          "HTTP/1.1 404 Not Found\r\n"
          "Content-Type: text/plain\r\n"
          "Content-Length: 9\r\n"
          "Connection: keep-alive\r\n"
          "\r\n"
          "Not Found";
      ClientState *client_state = (ClientState *)res.cli->data;
      uv_buf_t buf = uv_buf_init(const_cast<char *>(response), sizeof(response) - 1);
      uv_write(&client_state->write_handle, res.cli, &buf, 1, nullptr);
      delete &res;
    }
  }

  void invoke_ws_function(WebSocket &ws, const char *uri) {
    auto it = ws_routes_.find({uri});
    if (it != ws_routes_.end()) {
      it->second(ws);
    }
  }
};
