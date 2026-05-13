#include "ws.h"
#include "client_state.h"
#include "quickjs.h"
#include "runtime_context.h"
#include <cstdlib>

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

void sendFrame(WebSocket &ws, const uint8_t *payload, size_t len,
               uint8_t opcode = 0x2) {
  std::vector<uint8_t> frame;

  // First byte: FIN + opcode
  frame.push_back(0x80 | opcode); // FIN=1, no fragmentation

  // Length (server->client: no mask)
  if (len <= 125) {
    frame.push_back(len);
  } else if (len <= 65535) {
    frame.push_back(126);
    frame.push_back((len >> 8) & 0xFF);
    frame.push_back(len & 0xFF);
  } else {
    frame.push_back(127);
    for (int i = 7; i >= 0; i--) {
      frame.push_back((len >> (i * 8)) & 0xFF);
    }
  }

  frame.insert(frame.end(), payload, payload + len);

  uv_buf_t buf = uv_buf_init((char *)frame.data(), frame.size());
  uv_write_t *req = new uv_write_t;
  uv_write(req, (uv_stream_t *)ws.cli, &buf, 1,
           [](uv_write_t *req, int status) { delete req; });
}

void WebSocket::send_frame(const char *frame_payload) {
  std::cout << "Sending back some data " << frame_payload << std::endl;
  sendFrame(*this, (const uint8_t *)frame_payload, strlen(frame_payload), 1);
}
void WebSocket::onClose() {
  RuntimeContext *env = (RuntimeContext *)cli->loop->data;
  JSValue onclose = JS_GetPropertyStr(env->js_ctx, this->sock_js, "onClose");
  if (JS_IsFunction(env->js_ctx, onclose)) {
    JS_Call(env->js_ctx, onclose, this->sock_js, 0, nullptr);
  }
  JS_FreeValue(env->js_ctx, onclose);
}

int WebSocket::onMessage(const char *buffer, int buf_len) {
  Frame f;
  size_t op = parseFrame((const uint8_t *)buffer, buf_len, f);
  if (op == 0) {
    return op;
  }

  RuntimeContext *env = (RuntimeContext *)cli->loop->data;
  JSValue onmessage =
      JS_GetPropertyStr(env->js_ctx, this->sock_js, "onMessage");

  if (JS_IsFunction(env->js_ctx, onmessage)) {
    switch (f.opcode) {
    case 1: {
      std::string frame_data((char *)f.payload, f.len);
      JSValue payload = JS_NewString(env->js_ctx, frame_data.c_str());
      JSValue retVal =
          JS_Call(env->js_ctx, onmessage, this->sock_js, 1, &payload);
      JS_FreeValue(env->js_ctx, payload);
      JS_FreeValue(env->js_ctx, retVal);
      break;
    }

    default: {
      break;
    }
    }
  }
  JS_FreeValue(env->js_ctx, onmessage);

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
  uv_write_t *req = (uv_write_t *)malloc(sizeof(uv_write_t));
  req->data = client_state;

  uv_write(req, (uv_stream_t *)cli, &buf, 1, [](uv_write_t *req, int status) {
    if (status < 0) {
      std::cout << "Write to exchange WS key failed! " << uv_strerror(status)
                << std::endl;
    }
    free(req);
  });
  client_state->activeWs = this;
  client_state->conn_protocol = ConnectionProtocol::TYPE_WEB_SOCKET;
}