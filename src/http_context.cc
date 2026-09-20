#include "http_context.h"

void HttpContext::register_api_function(const char *method, const char *uri,
                                        Handler handler) {
  routes_[{method, uri}] = handler;
}

void HttpContext::register_ws_cb(const char *uri, WSHandler handler) {
  ws_routes_[uri] = handler;
}

void HttpContext::invoke_function(RequestObject &req, ResponseObject &res,
                                  const char *method, const char *uri) {
  auto it = routes_.find({req.verb, req.uri});
  if (it != routes_.end()) {
    it->second(req, res);
  } else {
    ResponseObject::resp_with_404(res);
  }
}

void HttpContext::invoke_ws_function(WebSocket &ws, const char *uri) {
  auto it = ws_routes_.find({uri});
  if (it != ws_routes_.end()) {
    it->second(ws);
  }
}