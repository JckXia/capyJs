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
                             Handler handler);

  void register_ws_cb(const char *uri, WSHandler handler);

  void invoke_function(RequestObject &req, ResponseObject &res,
                       const char *method, const char *uri);
                       
  void invoke_ws_function(WebSocket &ws, const char *uri);
};
