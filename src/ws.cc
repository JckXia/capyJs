#include "ws.h"
#include "client_state.h"
#include "quickjs.h"
#include "runtime_context.h"
#include "types.h"

struct Frame {
  bool fin;
  uint8_t opcode;
  uint8_t *payload;
  size_t len;
};

// Returns bytes consumed, 0 if need more data
size_t parseFrame(const uint8_t *data, size_t len, Frame &frame) {
  if (len < 2)
    return 0;

  frame.fin = data[0] & 0x80;
  frame.opcode = data[0] & 0x0F;

  bool masked = data[1] & 0x80;
  uint64_t payload_len = data[1] & 0x7F;

  size_t offset = 2;

  if (payload_len == 126) {
    if (len < 4)
      return 0;
    payload_len = (data[2] << 8) | data[3];
    offset = 4;
  } else if (payload_len == 127) {
    if (len < 10)
      return 0;
    payload_len = 0;
    for (int i = 0; i < 8; i++) {
      payload_len = (payload_len << 8) | data[2 + i];
    }
    offset = 10;
  }

  size_t mask_offset = offset;
  if (masked)
    offset += 4;

  size_t total = offset + payload_len;
  if (len < total)
    return 0; // need more data

  // Unmask in place (client->server is always masked)
  frame.payload = (uint8_t *)data + offset;
  frame.len = payload_len;

  if (masked) {
    const uint8_t *mask = data + mask_offset;
    for (size_t i = 0; i < payload_len; i++) {
      frame.payload[i] ^= mask[i % 4];
    }
  }

  return total;
}

void WebSocket::send_frame() {}

void WebSocket::onMessage(const std::string &frame_data) {
  RuntimeContext *env = (RuntimeContext *)cli->loop->data;
  JSValue js = this->sock_js;
  JSValue onmessage = JS_GetPropertyStr(env->js_ctx, js, "onMessage");
  if (JS_IsFunction(env->js_ctx, onmessage)) {
    JSValue payload = JS_NewString(env->js_ctx, frame_data.c_str());
    JS_Call(env->js_ctx, onmessage, js, 1, &payload);
    JS_FreeValue(env->js_ctx, payload);
  }
}

int WebSocket::onMessage(const char *buffer, int buf_len) {
  Frame f;
  size_t op = parseFrame((const uint8_t *)buffer, buf_len, f);
  if (op == 0) {
    return op;
  }

  RuntimeContext *env = (RuntimeContext *)cli->loop->data;
  JSValue js = this->sock_js;
  JSValue onmessage = JS_GetPropertyStr(env->js_ctx, js, "onMessage");

  if (JS_IsFunction(env->js_ctx, onmessage)) {
    switch (f.opcode) {
    case 1: {
      std::string frame_data((char *)f.payload, f.len);
      JSValue payload = JS_NewString(env->js_ctx, frame_data.c_str());
      JS_Call(env->js_ctx, onmessage, js, 1, &payload);
      JS_FreeValue(env->js_ctx, payload);
    }

    default: {
      break;
    }
    }
  }

  return op;
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