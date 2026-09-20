#include "http_context.h"
#include "server.h"

void HttpContext::register_api_function(const char *method, const char *uri,
                             Handler handler) {
    routes_[{method, uri}] = handler;
  }

void HttpContext::register_ws_cb(const char *uri, WSHandler handler) {
    ws_routes_[uri] = handler;
  }

  // Idea: We should probably not invoke uv_write manually here. Even an static method to Request would 
  //       prob be better
  void HttpContext::invoke_function(RequestObject &req, ResponseObject &res,
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
      client_state->write_handle.data = client_state;  
      uv_write(&client_state->write_handle, res.cli, &buf, 1, Server::on_static_write_cb);
      res.guard->release();
      
      delete &res;
    }
  }

  void HttpContext::invoke_ws_function(WebSocket &ws, const char *uri) {
    auto it = ws_routes_.find({uri});
    if (it != ws_routes_.end()) {
      it->second(ws);
    }
  }