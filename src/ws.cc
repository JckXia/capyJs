#include "ws.h"
#include "client_state.h"
#include "runtime_context.h"
#include "types.h"
#include "quickjs.h"

void WebSocket::send_frame() {}

void WebSocket::onMessage() {
    RuntimeContext *env = (RuntimeContext *)cli->loop->data;
    JSValue js = this->sock_js;
    JSValue onmessage = JS_GetPropertyStr(env->js_ctx, js, "onMessage");   
    if (JS_IsFunction(env->js_ctx, onmessage)) {
        JSValue payload = JS_NewString(env->js_ctx, "hello world");
        JS_Call(env->js_ctx, onmessage, js, 1, &payload);
        JS_FreeValue(env->js_ctx, payload);
    }
}

/**
HTTP/1.1 101 Switching Protocols
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
*/
void WebSocket::complete_protocol_upgrade_handshake(
    const std::string &exchange_key) {
  ClientState *client_state = (ClientState *)cli->data;
  RuntimeContext *ctx = (RuntimeContext *)cli->loop->data;
  if (client_state->closing == true) {
    return;
  }
  // std::string accept_key = wsAcceptKey(client_key);

  std::string response = "HTTP/1.1 101 Switching Protocols\r\n"
                         "Upgrade: websocket\r\n"
                         "Connection: Upgrade\r\n"
                         "Sec-WebSocket-Accept: " +
                         exchange_key +
                         "\r\n"
                         "\r\n";

  uv_buf_t buf = uv_buf_init((char *)response.c_str(), response.size());
  uv_write_t *req = (uv_write_t *)ctx->sock_alloc->alloc(
      sizeof(uv_write_t), AllocType::TYPE_UV_WRITE);
  req->data = client_state;

  uv_write(req, (uv_stream_t *)cli, &buf, 1, [](uv_write_t *req, int status) {
    if (status < 0) {
      // write failed, handle error
      std::cout << "Write to exchange WS key failed! " << uv_strerror(status)
                << std::endl;
    }
    ClientState *client_state = (ClientState *)req->data;
    RuntimeContext *ctx = (RuntimeContext *)req->handle->loop->data;
    ctx->sock_alloc->release(req);
    // ctx->sock_alloc->release(req);
  });
  client_state->activeWs = this;
  client_state->conn_protocol = ConnectionProtocol::TYPE_WEB_SOCKET;
 
}