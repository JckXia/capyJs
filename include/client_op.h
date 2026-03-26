#pragma once
#include "client_state.h"
#include "runtime_context.h"
namespace client_ops {

void recv_new_buffer(ClientState *client, ReadBuffer *buff) {
  if (buff != nullptr && client != nullptr) {
    client->recv_len += buff->len;
    if (client->recv_head == nullptr) {
      client->recv_head = buff;
    } else {
      client->recv_tail->next = buff;
    }
    client->recv_count += 1;
    client->recv_tail = buff;
  }
}

void clear_buffer(ClientState *client, RuntimeContext *ctx) {
  if (client == nullptr) {
    return;
  }
  while (client->recv_head != nullptr) {
    ReadBuffer *currHead = client->recv_head;
    ReadBuffer *next = client->recv_head->next;
    ctx->http_ctx->release_read_buffer(currHead);
    client->recv_head = next;
    client->recv_count -= 1;
  }

  client->recv_count = 0;
  client->recv_len = 0;
  client->recv_head = nullptr;
  client->recv_tail = nullptr;
}

int get_recv_packet_len(ClientState *client) { return client->recv_len; }
int get_recv_packet_count(ClientState *client) { return client->recv_count; }

void print_recv_buffer(ClientState *client) {
  ReadBuffer *wh = client->recv_head;
  char recv_buffer[client->recv_len + 1];
  size_t offset = 0;

  while (wh) {
    memcpy(recv_buffer + offset, wh->read_buffer, wh->len);
    offset += wh->len;
    wh = wh->next;
  }

  recv_buffer[client->recv_len] = '\0';
  std::cout << recv_buffer << std::endl;
}

// For HTTP/1.1 Request/Response objects. Stubbing for now untill we add llhttp
// in
void populate_request_object(ClientState *client, RequestObject &req) {
  memcpy(req.verb, "GET", 3);
  memcpy(req.uri, "/hello", 6);
  req.verb[3] = '\0';
  req.uri[6] = '\0';
}

} // namespace client_ops